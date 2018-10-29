// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;
import android.os.SystemClock;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.support.annotation.CallSuper;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.Display;
import android.view.Menu;
import android.view.Surface;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.view.WindowManager;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.DocumentModeAssassin;
import org.chromium.chrome.browser.upgrade.UpgradeActivity;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.lang.reflect.Field;

/**
 * An activity that talks with application and activity level delegates for async initialization.
 */
public abstract class AsyncInitializationActivity extends AppCompatActivity implements
        ChromeActivityNativeDelegate, BrowserParts {
    private static final String TAG = "AsyncInitActivity";
    protected final Handler mHandler;

    private final NativeInitializationController mNativeInitializationController =
            new NativeInitializationController(this);

    private final ActivityLifecycleDispatcher mLifecycleDispatcher =
            new ActivityLifecycleDispatcher();

    /** Time at which onCreate is called. This is realtime, counted in ms since device boot. */
    private long mOnCreateTimestampMs;

    /** Time at which onCreate is called. This is uptime, to be sent to native code. */
    private long mOnCreateTimestampUptimeMs;

    private ActivityWindowAndroid mWindowAndroid;
    private Bundle mSavedInstanceState;
    private int mCurrentOrientation = Surface.ROTATION_0;
    private boolean mDestroyed;
    private long mLastUserInteractionTime;
    private boolean mIsTablet;
    private boolean mHadWarmStart;
    private boolean mIsWarmOnResume;

    // Stores whether the activity was not resumed yet. Always false after the
    // first |onResume| call.
    private boolean mFirstResumePending = true;

    private boolean mStartupDelayed;
    private boolean mFirstDrawComplete;

    private Runnable mOnInflationCompleteCallback;

    public AsyncInitializationActivity() {
        mHandler = new Handler();
    }

    @CallSuper
    @Override
    protected void onDestroy() {
        mDestroyed = true;

        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }

        super.onDestroy();
        mLifecycleDispatcher.dispatchOnDestroy();
    }

    @CallSuper
    @Override
    @TargetApi(Build.VERSION_CODES.N)
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);

        // We override the smallestScreenWidthDp here for two reasons:
        // 1. To prevent multi-window from hiding the tabstrip when on a tablet.
        // 2. To ensure mIsTablet only needs to be set once. Since the override lasts for the life
        //    of the activity, it will never change via onConfigurationUpdated().
        // See crbug.com/588838, crbug.com/662338, crbug.com/780593.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(newBase);
            int targetSmallestScreenWidthDp =
                    DisplayUtil.pxToDp(display, DisplayUtil.getSmallestWidth(display));
            Configuration config = new Configuration();
            // Pre-Android O, fontScale gets initialized to 1 in the constructor. Set it to 0 so
            // that applyOverrideConfiguration() does not interpret it as an overridden value.
            // https://crbug.com/834191
            config.fontScale = 0;
            config.smallestScreenWidthDp = targetSmallestScreenWidthDp;
            applyOverrideConfiguration(config);
        }
    }

    @CallSuper
    @Override
    public void preInflationStartup() {
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(this);
        mHadWarmStart = LibraryLoader.getInstance().isInitialized();
        mLifecycleDispatcher.dispatchPreInflationStartup();
    }

    @Override
    public final void setContentViewAndLoadLibrary(Runnable onInflationCompleteCallback) {
        // Start loading libraries before triggerLayoutInflation(). This "hides" library loading
        // behind UI inflation and prevents stalling UI thread. See https://crbug.com/796957 for
        // details. Note that for optimal performance AsyncInitTaskRunner.startBackgroundTasks()
        // needs to start warmup renderer only after library is loaded.

        if (!mStartupDelayed) {
            // Kick off long running IO tasks that can be done in parallel.
            mNativeInitializationController.startBackgroundTasks(shouldAllocateChildConnection());
        }

        mOnInflationCompleteCallback = onInflationCompleteCallback;
        triggerLayoutInflation();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onSetContentView();
    }

    /** Controls the parameter of {@link NativeInitializationController#startBackgroundTasks}.*/
    @VisibleForTesting
    public boolean shouldAllocateChildConnection() {
        return true;
    }

    @CallSuper
    @Override
    public void postInflationStartup() {
        final View firstDrawView = getViewToBeDrawnBeforeInitializingNative();
        assert firstDrawView != null;
        ViewTreeObserver.OnPreDrawListener firstDrawListener =
                new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                firstDrawView.getViewTreeObserver().removeOnPreDrawListener(this);
                mFirstDrawComplete = true;
                if (!mStartupDelayed) {
                    onFirstDrawComplete();
                }
                return true;
            }
        };
        firstDrawView.getViewTreeObserver().addOnPreDrawListener(firstDrawListener);
        mLifecycleDispatcher.dispatchPostInflationStartup();
    }

    /**
     * @return The primary view that must have completed at least one draw before initializing
     *         native.  This must be non-null.
     */
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return findViewById(android.R.id.content);
    }

    @Override
    public void maybePreconnect() {
        try {
            TraceEvent.begin("maybePreconnect");
            Intent intent = getIntent();
            if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) return;
            String url = IntentHandler.getUrlFromIntent(intent);
            if (url == null) return;
            WarmupManager.getInstance().maybePreconnectUrlAndSubResources(
                    Profile.getLastUsedProfile(), url);
        } finally {
            TraceEvent.end("maybePreconnect");
        }
    }

    @Override
    public void initializeCompositor() { }

    @Override
    public void initializeState() { }

    @CallSuper
    @Override
    public void finishNativeInitialization() {
        // Set up the initial orientation of the device.
        checkOrientation();
        findViewById(android.R.id.content).addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(View v, int left, int top, int right, int bottom,
                            int oldLeft, int oldTop, int oldRight, int oldBottom) {
                        checkOrientation();
                    }
                });
        mNativeInitializationController.onNativeInitializationComplete();
        mLifecycleDispatcher.dispatchNativeInitializationFinished();
    }

    @CallSuper
    @Override
    public void onStartupFailure() {
        ProcessInitException e =
                new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_STARTUP_FAILED);
        ChromeApplication.reportStartupErrorAndExit(e);
    }

    /**
     * Extending classes should override {@link AsyncInitializationActivity#preInflationStartup()},
     * {@link AsyncInitializationActivity#triggerLayoutInflation()} and
     * {@link AsyncInitializationActivity#postInflationStartup()} instead of this call which will
     * be called on that order.
     */
    @Override
    @SuppressLint("MissingSuperCall")  // Called in onCreateInternal.
    protected final void onCreate(Bundle savedInstanceState) {
        TraceEvent.begin("AsyncInitializationActivity.onCreate()");
        onCreateInternal(savedInstanceState);
        TraceEvent.end("AsyncInitializationActivity.onCreate()");
    }

    /**
     * Called from onCreate() to give derived classes a chance to dispatch the intent using
     * {@link LaunchIntentDispatcher}. If the method returns anything other than Action.CONTINUE,
     * the activity is aborted. Default implementation returns Action.CONTINUE.
     * @param intent intent to dispatch
     * @return {@link LaunchIntentDispatcher.Action} to take
     */
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(Intent intent) {
        return LaunchIntentDispatcher.Action.CONTINUE;
    }

    private final void onCreateInternal(Bundle savedInstanceState) {
        initializeStartupMetrics();
        setIntent(validateIntent(getIntent()));

        @LaunchIntentDispatcher.Action
        int dispatchAction = maybeDispatchLaunchIntent(getIntent());
        if (dispatchAction != LaunchIntentDispatcher.Action.CONTINUE) {
            abortLaunch();
            return;
        }

        if (DocumentModeAssassin.getInstance().isMigrationNecessary()) {
            // Some Samsung devices load fonts from disk, crbug.com/691706.
            try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                super.onCreate(null);
            }

            // Kick the user to the MigrationActivity.
            UpgradeActivity.launchInstance(this, getIntent());

            // Don't remove this task -- it may be a DocumentActivity that exists only in Recents.
            finish();
            return;
        }

        Intent intent = getIntent();
        if (!isStartedUpCorrectly(intent)) {
            abortLaunch();
            return;
        }

        if (requiresFirstRunToBeCompleted(intent)
                && FirstRunFlowSequencer.launch(this, intent, false /* requiresBroadcast */,
                           shouldPreferLightweightFre(intent))) {
            abortLaunch();
            return;
        }

        // Some Samsung devices load fonts from disk, crbug.com/691706.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            super.onCreate(transformSavedInstanceStateForOnCreate(savedInstanceState));
        }
        mOnCreateTimestampMs = SystemClock.elapsedRealtime();
        mOnCreateTimestampUptimeMs = SystemClock.uptimeMillis();
        mSavedInstanceState = savedInstanceState;

        mWindowAndroid = createWindowAndroid();
        if (mWindowAndroid != null) {
            getWindowAndroid().restoreInstanceState(getSavedInstanceState());
        }

        mStartupDelayed = shouldDelayBrowserStartup();
        ChromeBrowserInitializer.getInstance(this).handlePreNativeStartup(this);
    }

    /**
     * This method is called very early on during Activity.onCreate. Subclassing activities should
     * use this to initialize their tracking metrics including things like Activity start time.
     */
    protected void initializeStartupMetrics() {}

    private void abortLaunch() {
        super.onCreate(null);
        ApiCompatibilityUtils.finishAndRemoveTask(this);
        overridePendingTransition(0, R.anim.no_anim);

        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1) {
            // On L ApiCompatibilityUtils.finishAndRemoveTask() sometimes fails, which causes
            // NPE in onStart() later, see crbug.com/781396. We can't let this activity to
            // start, and we don't want to crash either. So try finishing one more time and
            // suicide if that fails.
            if (!isFinishing()) {
                finish();
                if (!isFinishing()) Process.killProcess(Process.myPid());
            }
        }
    }

    /**
     * Call to begin loading the library, if it was delayed.
     */
    @CallSuper
    protected void startDelayedNativeInitialization() {
        assert mStartupDelayed;
        mStartupDelayed = false;

        // Kick off long running IO tasks that can be done in parallel.
        mNativeInitializationController.startBackgroundTasks(shouldAllocateChildConnection());

        if (mFirstDrawComplete) onFirstDrawComplete();
    }

    /**
     * @return Whether the native library initialization is delayed at this point.
     */
    protected boolean isStartupDelayed() {
        return mStartupDelayed;
    }

    /**
     * Creates an {@link ActivityWindowAndroid} to delegate calls to, if the Activity requires it.
     */
    @Nullable
    protected ActivityWindowAndroid createWindowAndroid() {
        return null;
    }

    /**
     * @return Whether the browser startup should be delayed. Note that changing this return value
     *         will have direct impact on startup performance.
     */
    protected boolean shouldDelayBrowserStartup() {
        return false;
    }

    /**
     * Allows subclasses to override the instance state passed to super.onCreate().
     * The original instance state will still be available via getSavedInstanceState().
     */
    protected Bundle transformSavedInstanceStateForOnCreate(Bundle savedInstanceState) {
        return savedInstanceState;
    }

    /**
     * Overriding this function is almost always wrong.
     * @return Whether or not the user needs to go through First Run before using this Activity.
     */
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        return true;
    }

    /**
     * Whether to use the Lightweight First Run Experience instead of the
     * non-Lightweight First Run Experience.
     */
    protected boolean shouldPreferLightweightFre(Intent intent) {
        return false;
    }

    /**
     * Whether or not the Activity was started up via a valid Intent.
     */
    protected boolean isStartedUpCorrectly(Intent intent) {
        return true;
    }

    /**
     * Validates the intent that started this activity.
     * @return The validated intent.
     */
    protected Intent validateIntent(final Intent intent) {
        return intent;
    }

    /**
     * @return The elapsed real time for the activity creation in ms.
     */
    protected long getOnCreateTimestampUptimeMs() {
        return mOnCreateTimestampUptimeMs;
    }

    /**
     * @return The uptime for the activity creation in ms.
     */
    protected long getOnCreateTimestampMs() {
        return mOnCreateTimestampMs;
    }

    /**
     * @return The saved bundle for the last recorded state.
     */
    protected Bundle getSavedInstanceState() {
        return mSavedInstanceState;
    }

    /**
     * Resets the saved state and makes it unavailable for the rest of the activity lifecycle.
     */
    protected void resetSavedInstanceState() {
        mSavedInstanceState = null;
    }

    @CallSuper
    @Override
    public void onStart() {
        super.onStart();
        mNativeInitializationController.onStart();
    }

    @CallSuper
    @Override
    public void onResume() {
        super.onResume();

        // Start by setting the launch as cold or warm. It will be used in some resume handlers.
        mIsWarmOnResume = !mFirstResumePending || hadWarmStart();
        mFirstResumePending = false;

        mNativeInitializationController.onResume();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onResume();
    }

    @CallSuper
    @Override
    public void onPause() {
        mNativeInitializationController.onPause();
        super.onPause();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onPause();
    }

    @CallSuper
    @Override
    public void onStop() {
        super.onStop();
        mNativeInitializationController.onStop();
    }

    @CallSuper
    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;
        mNativeInitializationController.onNewIntent(intent);
        setIntent(intent);
    }

    @CallSuper
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mNativeInitializationController.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public final void onCreateWithNative() {
        try {
            ChromeBrowserInitializer.getInstance(this).handlePostNativeStartup(true, this);
        } catch (ProcessInitException e) {
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }

    @CallSuper
    @Override
    public void onStartWithNative() {
        mLifecycleDispatcher.dispatchOnStartWithNative();
    }

    @CallSuper
    @Override
    public void onResumeWithNative() {
        mLifecycleDispatcher.dispatchOnResumeWithNative();
    }

    @CallSuper
    @Override
    public void onPauseWithNative() {
        mLifecycleDispatcher.dispatchOnPauseWithNative();
    }

    @CallSuper
    @Override
    public void onStopWithNative() {
        mLifecycleDispatcher.dispatchOnStopWithNative();
    }

    @Override
    public boolean isActivityDestroyed() {
        return mDestroyed;
    }

    @Override
    public boolean isActivityFinishing() {
        return isFinishing();
    }

    @Override
    public abstract boolean shouldStartGpuProcess();

    @CallSuper
    @Override
    public void onContextMenuClosed(Menu menu) {
        if (mWindowAndroid != null) mWindowAndroid.onContextMenuClosed();
    }

    private void onFirstDrawComplete() {
        assert mFirstDrawComplete;
        assert !mStartupDelayed;

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mNativeInitializationController.firstDrawComplete();
            }
        });
    }

    @Override
    public void onNewIntentWithNative(Intent intent) { }

    @Override
    public Intent getInitialIntent() {
        return getIntent();
    }

    /**
     * @return A {@link ActivityWindowAndroid} instance.  May be null if one was not created.
     */
    @Nullable
    public ActivityWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * This will handle passing {@link Intent} results back to the {@link WindowAndroid}.  It will
     * return whether or not the {@link WindowAndroid} has consumed the event or not.
     */
    @CallSuper
    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (mWindowAndroid != null
                && mWindowAndroid.onActivityResult(requestCode, resultCode, intent)) {
            return true;
        }
        super.onActivityResult(requestCode, resultCode, intent);
        return false;
    }

    @CallSuper
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid != null) {
            if (mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults)) {
                return;
            }
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @CallSuper
    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mWindowAndroid != null) mWindowAndroid.saveInstanceState(outState);
    }

    /**
     * @return Whether the activity is running in tablet mode.
     */
    public boolean isTablet() {
        return mIsTablet;
    }

    /**
     * @return Whether the activity had a warm start because the native library was already fully
     *     loaded and initialized.
     */
    public boolean hadWarmStart() {
        return mHadWarmStart;
    }

    /**
     * This returns true if the activity was started warm (native library loaded and initialized) or
     * if a cold starts have been completed by the time onResume is/will be called.
     * This is useful to distinguish between the case where an already running instance of Chrome is
     * being brought back to the foreground from the case where Chrome is started, in order to avoid
     * contention on browser startup
     * @return Whether the activity is warm in onResume.
     */
    public boolean isWarmOnResume() {
        return mIsWarmOnResume;
    }

    @Override
    public void onUserInteraction() {
        mLastUserInteractionTime = SystemClock.elapsedRealtime();
    }

    /**
     * @return timestamp when the last user interaction was made.
     */
    public long getLastUserInteractionTime() {
        return mLastUserInteractionTime;
    }

    /**
     * Called when the orientation of the device changes.  The orientation is checked/detected on
     * root view layouts.
     * @param orientation One of {@link Surface#ROTATION_0} (no rotation),
     *                    {@link Surface#ROTATION_90}, {@link Surface#ROTATION_180}, or
     *                    {@link Surface#ROTATION_270}.
     */
    protected void onOrientationChange(int orientation) {
    }

    private void checkOrientation() {
        WindowManager wm = getWindowManager();
        if (wm == null) return;

        Display display = wm.getDefaultDisplay();
        if (display == null) return;

        int oldOrientation = mCurrentOrientation;
        mCurrentOrientation = display.getRotation();

        if (oldOrientation != mCurrentOrientation) onOrientationChange(mCurrentOrientation);
    }

    /**
     * Removes the window background.
     */
    protected void removeWindowBackground() {
        boolean removeWindowBackground = true;
        try {
            Field field = Settings.Secure.class.getField(
                    "ACCESSIBILITY_DISPLAY_MAGNIFICATION_ENABLED");
            field.setAccessible(true);

            if (field.getType() == String.class) {
                String accessibilityMagnificationSetting = (String) field.get(null);
                // When Accessibility magnification is turned on, setting a null window
                // background causes the overlaid android views to stretch when panning.
                // (crbug/332994)
                if (Settings.Secure.getInt(
                        getContentResolver(), accessibilityMagnificationSetting) == 1) {
                    removeWindowBackground = false;
                }
            }
        } catch (SettingNotFoundException | NoSuchFieldException | IllegalAccessException
                | IllegalArgumentException ignore) {
            // Window background is removed if an exception occurs.
        }
        if (removeWindowBackground) getWindow().setBackgroundDrawable(null);
    }

    /**
     * Extending classes should implement this, inflate the layout, set the content view and then
     * call {@link #onInitialLayoutInflationComplete}.
     */
    protected abstract void triggerLayoutInflation();

    /**
     * Once inflation is complete, this runs the callback to inform ChromeBrowserInitializer of this
     * and to start the post-inflation pre-native startup.
     */
    @CallSuper
    protected void onInitialLayoutInflationComplete() {
        if (mOnInflationCompleteCallback == null) return;
        mOnInflationCompleteCallback.run();
        mOnInflationCompleteCallback = null;
    }

    /**
     * @return {@link ActivityLifecycleDispatcher} associated with this activity.
     */
    protected ActivityLifecycleDispatcher getLifecycleDispatcher() {
        return mLifecycleDispatcher;
    }

    /**
     * Lollipop (pre-MR1) makeTaskLaunchBehind() workaround.
     *
     * Our activity's surface is destroyed at the end of the new activity animation
     * when ActivityOptions.makeTaskLaunchBehind() is used, which causes a crash.
     * Making everything invisible when paused prevents the crash, since view changes
     * will not trigger draws to the missing surface. However, we need to wait until
     * after the first draw to make everything invisible, as the activity launch
     * animation needs a full frame (or it will delay the animation excessively).
     */
    private final LaunchBehindWorkaround mLaunchBehindWorkaround =
            (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP)
                    ? new LaunchBehindWorkaround()
                    : null;

    private class LaunchBehindWorkaround {
        private boolean mPaused;

        private View getDecorView() {
            return getWindow().getDecorView();
        }

        private ViewTreeObserver getViewTreeObserver() {
            return getDecorView().getViewTreeObserver();
        }

        private void onPause() {
            mPaused = true;
        }

        public void onResume() {
            mPaused = false;
            getDecorView().setVisibility(View.VISIBLE);
        }

        public void onSetContentView() {
            getViewTreeObserver().addOnPreDrawListener(mPreDrawListener);
        }

        // Note, we probably want onDrawListener here, but it isn't being called
        // when I add this to the decorView. However, it should be the same for
        // this purpose as long as no other pre-draw listener returns false.
        private final OnPreDrawListener mPreDrawListener = new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mPaused) {
                            getDecorView().setVisibility(View.GONE);
                        }
                        getViewTreeObserver().removeOnPreDrawListener(mPreDrawListener);
                    }
                });
                return true;
            }
        };
    }
}
