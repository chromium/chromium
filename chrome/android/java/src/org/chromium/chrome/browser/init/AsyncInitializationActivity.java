// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.annotation.SuppressLint;
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
import android.view.Display;
import android.view.Menu;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.view.WindowManager;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.lang.reflect.Field;

/**
 * An activity that talks with application and activity level delegates for async initialization.
 */
public abstract class AsyncInitializationActivity extends ChromeBaseAppCompatActivity
        implements ChromeActivityNativeDelegate, BrowserParts, ModalDialogManagerHolder {
    private static final String TAG = "AsyncInitActivity";
    protected final Handler mHandler;

    private final NativeInitializationController mNativeInitializationController =
            new NativeInitializationController(this);

    private final ActivityLifecycleDispatcherImpl mLifecycleDispatcher =
            new ActivityLifecycleDispatcherImpl();
    private final MultiWindowModeStateDispatcherImpl mMultiWindowModeStateDispatcher =
            new MultiWindowModeStateDispatcherImpl(this);

    /** Time at which onCreate is called. This is realtime, counted in ms since device boot. */
    private long mOnCreateTimestampMs;

    /** Time at which onCreate is called. This is uptime, to be sent to native code. */
    private long mOnCreateTimestampUptimeMs;

    private ActivityWindowAndroid mWindowAndroid;
    private ModalDialogManager mModalDialogManager;
    private Bundle mSavedInstanceState;
    private int mCurrentOrientation;
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
    private boolean mInitialLayoutInflationComplete;

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

        if (mModalDialogManager != null) {
            mModalDialogManager.destroy();
            mModalDialogManager = null;
        }

        super.onDestroy();
        mLifecycleDispatcher.dispatchOnDestroy();
    }

    @Override
    @CallSuper
    protected boolean applyOverrides(Context baseContext, Configuration overrideConfig) {
        super.applyOverrides(baseContext, overrideConfig);

        // We override the smallestScreenWidthDp here for two reasons:
        // 1. To prevent multi-window from hiding the tabstrip when on a tablet.
        // 2. To ensure mIsTablet only needs to be set once. Since the override lasts for the life
        //    of the activity, it will never change via onConfigurationUpdated().
        // See crbug.com/588838, crbug.com/662338, crbug.com/780593.
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(baseContext);
        int targetSmallestScreenWidthDp =
                DisplayUtil.pxToDp(display, DisplayUtil.getSmallestWidth(display));
        overrideConfig.smallestScreenWidthDp = targetSmallestScreenWidthDp;
        return true;
    }

    @Override
    public final void preInflationStartup() {
        performPreInflationStartup();
    }

    /**
     * Perform pre-inflation startup for the activity. Sub-classes providing custom pre-inflation
     * startup logic should override this method.
     */
    @CallSuper
    protected void performPreInflationStartup() {
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(this);
        mHadWarmStart = LibraryLoader.getInstance().isInitialized();
        // TODO(https://crbug.com/948745): Dispatch in #preInflationStartup instead so that
        // subclass's #performPreInflationStartup has executed before observers are notified.
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
        // If a spare WebContents exists, a child connection has already been allocated that will be
        // used by the next created tab.
        return !WarmupManager.getInstance().hasSpareWebContents();
    }

    @Override
    public final void postInflationStartup() {
        performPostInflationStartup();
        mLifecycleDispatcher.dispatchPostInflationStartup();
    }

    /**
     * Perform post-inflation startup for the activity. Sub-classes providing custom post-inflation
     * startup logic should override this method.
     */
    @CallSuper
    protected void performPostInflationStartup() {
        View firstDrawView = getViewToBeDrawnBeforeInitializingNative();
        assert firstDrawView != null;
        FirstDrawDetector.waitForFirstDraw(firstDrawView, () -> {
            mFirstDrawComplete = true;
            if (!mStartupDelayed) {
                onFirstDrawComplete();
            }
        });
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
        throw new ProcessInitException(LoaderErrors.NATIVE_STARTUP_FAILED);
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
            abortLaunch(dispatchAction);
            return;
        }

        Intent intent = getIntent();
        if (!isStartedUpCorrectly(intent)) {
            abortLaunch(LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK);
            return;
        }

        if (requiresFirstRunToBeCompleted(intent)
                && FirstRunFlowSequencer.launch(this, intent, false /* requiresBroadcast */,
                        shouldPreferLightweightFre(intent))) {
            abortLaunch(LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK);
            return;
        }

        // Some Samsung devices load fonts from disk, crbug.com/691706.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            super.onCreate(transformSavedInstanceStateForOnCreate(savedInstanceState));
        }
        mOnCreateTimestampMs = SystemClock.elapsedRealtime();
        mOnCreateTimestampUptimeMs = SystemClock.uptimeMillis();
        mSavedInstanceState = savedInstanceState;

        mWindowAndroid = createWindowAndroid();
        if (mWindowAndroid != null) {
            getWindowAndroid().restoreInstanceState(getSavedInstanceState());
        }
        mModalDialogManager = createModalDialogManager();

        mStartupDelayed = shouldDelayBrowserStartup();
        ChromeBrowserInitializer.getInstance(this).handlePreNativeStartup(this);
    }

    /**
     * This method is called very early on during Activity.onCreate. Subclassing activities should
     * use this to initialize their tracking metrics including things like Activity start time.
     */
    protected void initializeStartupMetrics() {}

    private void abortLaunch(@LaunchIntentDispatcher.Action int dispatchAction) {
        super.onCreate(null);
        if (dispatchAction == LaunchIntentDispatcher.Action.FINISH_ACTIVITY) {
            finish();
            return;
        } else {
            assert dispatchAction == LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK;
            ApiCompatibilityUtils.finishAndRemoveTask(this);

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
        overridePendingTransition(0, R.anim.no_anim);
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
    public Bundle getSavedInstanceState() {
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
        mLifecycleDispatcher.onCreateWithNative();
        ChromeBrowserInitializer.getInstance(this).handlePostNativeStartup(true, this);
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
    public boolean isActivityFinishingOrDestroyed() {
        return mDestroyed || isFinishing();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mLifecycleDispatcher.dispatchOnConfigurationChanged(newConfig);
    }

    @Override
    public void onMultiWindowModeChanged(boolean inMultiWindowMode) {
        super.onMultiWindowModeChanged(inMultiWindowMode);
        mMultiWindowModeStateDispatcher.dipatchMultiWindowModeChanged(inMultiWindowMode);
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
        TraceEvent.instant("onFirstDrawComplete");
        mNativeInitializationController.firstDrawComplete();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) { }

    @Override
    public Intent getInitialIntent() {
        return getIntent();
    }

    /**
     * Creates an {@link ActivityWindowAndroid} to delegate calls to, if the Activity requires it.
     */
    @Nullable
    protected ActivityWindowAndroid createWindowAndroid() {
        return null;
    }

    /**
     * @return A {@link ActivityWindowAndroid} instance.  May be null if one was not created.
     */
    @Nullable
    public ActivityWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return The {@link ModalDialogManager} created for this class.
     */
    @Nullable
    protected ModalDialogManager createModalDialogManager() {
        return null;
    }

    /**
     * @return The {@link ModalDialogManager} that manages the display of modal dialogs (e.g.
     *         JavaScript dialogs).
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    /**
     * Overrides the originally created modal dialog manager.
     */
    public void overrideModalDialogManager(ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
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
        mLifecycleDispatcher.dispatchOnActivityResultWithNative(requestCode, resultCode, intent);
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

        mLifecycleDispatcher.dispatchOnSaveInstanceState(outState);
    }

    @CallSuper
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        mLifecycleDispatcher.dispatchOnWindowFocusChanged(hasFocus);
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
     * @param orientation One of {@link Configuration#ORIENTATION_PORTRAIT} or
     *                    {@link Configuration#ORIENTATION_LANDSCAPE}.
     */
    protected void onOrientationChange(int orientation) {
    }

    private void checkOrientation() {
        WindowManager wm = getWindowManager();
        if (wm == null) return;

        Display display = wm.getDefaultDisplay();
        if (display == null) return;

        int oldOrientation = mCurrentOrientation;
        mCurrentOrientation = getResources().getConfiguration().orientation;

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
        mInitialLayoutInflationComplete = true;
    }

    /**
     * Returns whether initial inflation is complete.
     */
    public boolean isInitialLayoutInflationComplete() {
        return mInitialLayoutInflationComplete;
    }

    /**
     * @return {@link ActivityLifecycleDispatcher} associated with this activity.
     */
    public ActivityLifecycleDispatcher getLifecycleDispatcher() {
        return mLifecycleDispatcher;
    }

    /**
     * @return {@link MultiWindowModeStateDispatcher} associated with this activity.
     */
    public MultiWindowModeStateDispatcher getMultiWindowModeStateDispatcher() {
        return mMultiWindowModeStateDispatcher;
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
