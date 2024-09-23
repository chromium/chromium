// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.view.Display;
import android.view.Menu;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.util.FirstDrawDetector;
import org.chromium.ui.base.ActivityIntentRequestTrackerDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;

/**
 * An activity that talks with application and activity level delegates for async initialization.
 */
public abstract class AsyncInitializationActivity extends ChromeBaseAppCompatActivity
        implements ChromeActivityNativeDelegate, BrowserParts, ActivityLifecycleDispatcherProvider {
    @VisibleForTesting
    public static final String FIRST_DRAW_COMPLETED_TIME_MS_UMA = "FirstDrawCompletedTime";

    public static final String TAG_MULTI_INSTANCE = "MultiInstance";

    protected final Handler mHandler;

    private final NativeInitializationController mNativeInitializationController =
            new NativeInitializationController(this);

    private final ActivityLifecycleDispatcherImpl mLifecycleDispatcher =
            new ActivityLifecycleDispatcherImpl(this);
    private final MultiWindowModeStateDispatcherImpl mMultiWindowModeStateDispatcher =
            new MultiWindowModeStateDispatcherImpl(this);
    private final IntentRequestTracker mIntentRequestTracker;

    /** Time at which onCreate is called. This is realtime, counted in ms since device boot. */
    private long mOnCreateTimestampMs;

    /** Time at which onPause is called. */
    private long mOnPauseTimestampMs;

    /**
     * Time at which onPause is called before the activity is recreated due to unfolding. The
     * timestamp is captured only if recreation starts when the activity is not in stopped state.
     */
    private long mOnPauseBeforeFoldRecreateTimestampMs;

    private ActivityWindowAndroid mWindowAndroid;
    private OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
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

    // See enableHardwareAcceleration()
    private boolean mSetWindowHWA;

    private static boolean sInterceptMoveTaskToBackForTesting;
    private static boolean sBackInterceptedForTesting;

    public AsyncInitializationActivity() {
        mHandler = new Handler();
        mIntentRequestTracker =
                IntentRequestTracker.createFromDelegate(
                        new ActivityIntentRequestTrackerDelegate(this) {
                            @Override
                            public boolean onCallbackNotFoundError(String error) {
                                return onIntentCallbackNotFoundError(error);
                            }
                        });
    }

    /** Get the tracker of this activity's intents. */
    public IntentRequestTracker getIntentRequestTracker() {
        return mIntentRequestTracker;
    }

    @CallSuper
    @Override
    protected void onDestroy() {
        mDestroyed = true;
        mLifecycleDispatcher.onDestroyStarted();

        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
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
        overrideConfig.smallestScreenWidthDp =
                DisplayUtil.getCurrentSmallestScreenWidth(baseContext);
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
        // TODO(crbug.com/40621278): Dispatch in #preInflationStartup instead so that
        // subclass's #performPreInflationStartup has executed before observers are notified.
        mLifecycleDispatcher.dispatchPreInflationStartup();
    }

    @Override
    public final void setContentViewAndLoadLibrary(Runnable onInflationCompleteCallback) {
        mOnInflationCompleteCallback = onInflationCompleteCallback;

        // Start loading libraries. It happens before triggerLayoutInflation(). This "hides" library
        // loading behind UI inflation and prevents stalling UI thread.
        // See https://crbug.com/796957 for details. Note that for optimal performance
        // AsyncInitTaskRunner.startBackgroundTasks() needs to start warm up renderer only after
        // library is loaded.

        if (!mStartupDelayed) {
            // Kick off long running IO tasks that can be done in parallel.
            mNativeInitializationController.startBackgroundTasks(shouldAllocateChildConnection());
        }

        triggerLayoutInflation();
    }

    /** Controls the parameter of {@link NativeInitializationController#startBackgroundTasks}. */
    @VisibleForTesting
    public boolean shouldAllocateChildConnection() {
        // If a spare WebContents exists, a child connection has already been allocated that will be
        // used by the next created tab.
        return !WarmupManager.getInstance().hasSpareWebContents();
    }

    @Override
    public final void postInflationStartup() {
        performPostInflationStartup();
        mLifecycleDispatcher.dispatchOnInflationComplete();
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
        FirstDrawDetector.waitForFirstDraw(
                firstDrawView,
                () -> {
                    mFirstDrawComplete = true;
                    BrowserUiUtils.recordHistogram(
                            FIRST_DRAW_COMPLETED_TIME_MS_UMA,
                            SystemClock.elapsedRealtime() - getOnCreateTimestampMs());
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
            // Blocking pre-connect for all off-the-record profiles.
            if (IntentHandler.hasAnyIncognitoExtra(intent.getExtras())) return;
            assert getProfileProviderSupplier().hasValue();
            getProfileProviderSupplier()
                    .runSyncOrOnAvailable(
                            (profileProvider) -> {
                                WarmupManager.getInstance()
                                        .maybePreconnectUrlAndSubResources(
                                                profileProvider.getOriginalProfile(), url);
                            });
        } finally {
            TraceEvent.end("maybePreconnect");
        }
    }

    @Override
    public void initializeCompositor() {}

    @Override
    public void initializeState() {}

    @CallSuper
    @Override
    public void finishNativeInitialization() {
        // Set up the initial orientation of the device.
        checkOrientation();
        findViewById(android.R.id.content)
                .addOnLayoutChangeListener(
                        new View.OnLayoutChangeListener() {
                            @Override
                            public void onLayoutChange(
                                    View v,
                                    int left,
                                    int top,
                                    int right,
                                    int bottom,
                                    int oldLeft,
                                    int oldTop,
                                    int oldRight,
                                    int oldBottom) {
                                checkOrientation();
                            }
                        });
        mNativeInitializationController.onNativeInitializationComplete();
        mLifecycleDispatcher.dispatchNativeInitializationFinished();
    }

    @CallSuper
    @Override
    public void onStartupFailure(Exception failureCause) {
        throw new ProcessInitException(LoaderErrors.NATIVE_STARTUP_FAILED, failureCause);
    }

    @Override
    public void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {}

    /**
     * Extending classes should override {@link AsyncInitializationActivity#preInflationStartup()},
     * {@link AsyncInitializationActivity#triggerLayoutInflation()} and {@link
     * AsyncInitializationActivity#postInflationStartup()} instead of this call which will be called
     * on that order.
     */
    @Override
    @SuppressLint("MissingSuperCall") // Called in onCreateInternal.
    protected final void onCreate(Bundle savedInstanceState) {
        TraceEvent.begin("AsyncInitializationActivity.onCreate()");
        onPreCreate();
        boolean willCreate = onCreateInternal(savedInstanceState);
        if (!willCreate) {
            onAbortCreate();
        } else {
            onPostCreate();
        }
        TraceEvent.end("AsyncInitializationActivity.onCreate()");
    }

    /**
     * Override to perform operations in the first opportunity after the framework calls
     * {@link #onCreate}. Note the activity may still be aborted by {@link #onCreateInternal}.
     */
    protected void onPreCreate() {}

    /**
     * Override to perform operations after the activity's creation is aborted by {@link
     * #onCreateInternal}.
     */
    protected void onAbortCreate() {}

    /**
     * Override to perform operations after the framework successfully calls {@link
     * #onCreateInternal}. This method is used in the ChromeActivity derived class to increment the
     * "Chrome.UMA.OnPostCreateCounter2" counter for the histogram
     * UMA.AndroidPreNative.ChromeActivityCounter2.
     */
    protected void onPostCreate() {}

    /**
     * Called from onCreate() to give derived classes a chance to dispatch the intent using
     * {@link LaunchIntentDispatcher}. If the method returns anything other than Action.CONTINUE,
     * the activity is aborted. Default implementation returns Action.CONTINUE.
     * @param intent intent to dispatch
     * @return {@link LaunchIntentDispatcher.Action} to take
     */
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(
            Intent intent, Bundle savedInstanceState) {
        return LaunchIntentDispatcher.Action.CONTINUE;
    }

    /**
     * @return true if will proceed with Activity creation, false if will abort.
     */
    private boolean onCreateInternal(Bundle savedInstanceState) {
        initializeStartupMetrics();
        setIntent(IntentHandler.rewriteFromHistoryIntent(getIntent()));

        @LaunchIntentDispatcher.Action
        int dispatchAction = maybeDispatchLaunchIntent(getIntent(), savedInstanceState);
        if (dispatchAction != LaunchIntentDispatcher.Action.CONTINUE) {
            abortLaunch(dispatchAction);
            return false;
        }

        Intent intent = getIntent();
        if (!isStartedUpCorrectly(intent)) {
            abortLaunch(LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK);
            return false;
        }

        if (requiresFirstRunToBeCompleted(intent)
                && FirstRunFlowSequencer.launch(this, intent, shouldPreferLightweightFre(intent))) {
            abortLaunch(LaunchIntentDispatcher.Action.FINISH_ACTIVITY);
            return false;
        }

        super.onCreate(transformSavedInstanceStateForOnCreate(savedInstanceState));
        mOnCreateTimestampMs = SystemClock.elapsedRealtime();
        mSavedInstanceState = savedInstanceState;

        mWindowAndroid = createWindowAndroid();
        mIntentRequestTracker.restoreInstanceState(getSavedInstanceState());
        mProfileProviderSupplier = createProfileProvider();

        mStartupDelayed = shouldDelayBrowserStartup();

        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(this);
        return true;
    }

    /**
     * A custom error handler for {@link IntentRequestTracker}. When false, the tracker will use
     * the default error handler. Derived classes can override this method to customize the error
     * handling.
     */
    protected boolean onIntentCallbackNotFoundError(String error) {
        return false;
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
            finishAndRemoveTask();
        }
        overridePendingTransition(0, R.anim.no_anim);
    }

    /** Call to begin loading the library, if it was delayed. */
    @CallSuper
    protected void startDelayedNativeInitialization() {
        assert mStartupDelayed;
        mStartupDelayed = false;

        // Kick off long running IO tasks that can be done in parallel.
        mNativeInitializationController.startBackgroundTasks(shouldAllocateChildConnection());

        if (mFirstDrawComplete) onFirstDrawComplete();
    }

    public void startDelayedNativeInitializationForTests() {
        mStartupDelayed = true;
        startDelayedNativeInitialization();
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

    /** Whether or not the Activity was started up via a valid Intent. */
    protected boolean isStartedUpCorrectly(Intent intent) {
        return true;
    }

    /**
     * @return The uptime for the activity creation in ms.
     */
    protected long getOnCreateTimestampMs() {
        return mOnCreateTimestampMs;
    }

    /**
     * @return The timestamp for OnPause event before activity restarts due to unfolding in ms.
     */
    protected long getOnPauseBeforeFoldRecreateTimestampMs() {
        try (TraceEvent e =
                TraceEvent.scoped(
                        "AsyncInit.getOnPauseBeforeFoldRecreateTimestampMs",
                        Long.toString(mOnPauseBeforeFoldRecreateTimestampMs))) {
            return mOnPauseBeforeFoldRecreateTimestampMs;
        }
    }

    protected void setOnPauseBeforeFoldRecreateTimestampMs() {
        try (TraceEvent e =
                TraceEvent.scoped(
                        "AsyncInit.setOnPauseBeforeFoldRecreateTimestampMs",
                        Long.toString(mOnPauseTimestampMs))) {
            mOnPauseBeforeFoldRecreateTimestampMs = mOnPauseTimestampMs;
        }
    }

    /**
     * @return The saved bundle for the last recorded state.
     */
    public Bundle getSavedInstanceState() {
        return mSavedInstanceState;
    }

    /** Resets the saved state and makes it unavailable for the rest of the activity lifecycle. */
    protected void resetSavedInstanceState() {
        mSavedInstanceState = null;
    }

    @CallSuper
    @Override
    public void onStart() {
        super.onStart();
        mNativeInitializationController.onStart();

        // Since this activity is being started, the FRE should have been handled somehow already.
        Intent intent = getIntent();
        if (FirstRunFlowSequencer.checkIfFirstRunIsNecessary(
                        shouldPreferLightweightFre(intent), intent)
                && requiresFirstRunToBeCompleted(intent)) {
            throw new IllegalStateException(
                    "The app has not completed the FRE yet "
                            + getClass().getName()
                            + " is trying to start.");
        }
    }

    @CallSuper
    @Override
    public void onResume() {
        super.onResume();

        // Start by setting the launch as cold or warm. It will be used in some resume handlers.
        mIsWarmOnResume = !mFirstResumePending || hadWarmStart();
        mFirstResumePending = false;

        SimpleStartupForegroundSessionDetector.onTransitionToForeground();
        mNativeInitializationController.onResume();
    }

    @CallSuper
    @Override
    public void onPause() {
        mOnPauseTimestampMs = SystemClock.uptimeMillis();
        SimpleStartupForegroundSessionDetector.discardSession();
        mNativeInitializationController.onPause();
        super.onPause();
    }

    @CallSuper
    @Override
    public void onStop() {
        super.onStop();
        mNativeInitializationController.onStop();
    }

    @CallSuper
    @Override
    @SuppressLint("MissingSuperCall") // Empty method in parent Activity class.
    public void onNewIntent(Intent intent) {
        if (intent == null) return;
        if (ShareHelper.isCleanerIntent(intent)) return;
        mNativeInitializationController.onNewIntent(intent);
        setIntent(intent);
    }

    @CallSuper
    @Override
    @SuppressLint("MissingSuperCall") // Empty method in parent Activity class.
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mNativeInitializationController.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public final void onCreateWithNative() {
        mLifecycleDispatcher.onCreateWithNative();
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, this);
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

    @CallSuper
    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();
        mLifecycleDispatcher.dispatchOnUserLeaveHint();
    }

    @Override
    public boolean isActivityFinishingOrDestroyed() {
        return mDestroyed || isFinishing();
    }

    /**
     * Every child class wanting to perform tasks on configuration changed should override
     * {@link #performOnConfigurationChanged(Configuration)} instead.
     * @param newConfig The new configuration.
     */
    @Override
    public final void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        performOnConfigurationChanged(newConfig);
        mLifecycleDispatcher.dispatchOnConfigurationChanged(newConfig);
    }

    /**
     * Handle an {@link #onConfigurationChanged(Configuration)} event.
     * @param newConfig The new configuration.
     */
    @CallSuper
    public void performOnConfigurationChanged(Configuration newConfig) {}

    @Override
    public void onMultiWindowModeChanged(boolean inMultiWindowMode) {
        super.onMultiWindowModeChanged(inMultiWindowMode);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(inMultiWindowMode);
    }

    @Override
    public abstract boolean shouldStartGpuProcess();

    @CallSuper
    @Override
    public void onContextMenuClosed(Menu menu) {
        if (mWindowAndroid != null) mWindowAndroid.onContextMenuClosed();
    }

    /**
     * Called when the content view gets drawn for the first time. See {@link FirstDrawDetector} for
     * details on the exact signals used to call this.
     */
    @CallSuper
    protected void onFirstDrawComplete() {
        assert mFirstDrawComplete;
        assert !mStartupDelayed;
        TraceEvent.instant("onFirstDrawComplete");
        mNativeInitializationController.firstDrawComplete();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {}

    @Override
    public Intent getInitialIntent() {
        return getIntent();
    }

    /**
     * Creates an {@link ActivityWindowAndroid} to delegate calls to, if the Activity requires it.
     */
    protected @Nullable ActivityWindowAndroid createWindowAndroid() {
        return null;
    }

    /**
     * @return A {@link ActivityWindowAndroid} instance. May be null if one was not created.
     */
    public @Nullable ActivityWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * Handles creating the {@link ProfileProvider} for the given Activity.
     *
     * <p>Implementers should not assume the native library is loaded when this is triggered.
     */
    protected abstract @NonNull OneshotSupplier<ProfileProvider> createProfileProvider();

    /** Return a supplier for the ProfileProvider. */
    public OneshotSupplier<ProfileProvider> getProfileProviderSupplier() {
        // TODO(crbug.com/40275690): Convert to a thrown exception if no asserts are discovered.
        assert mProfileProviderSupplier != null;
        return mProfileProviderSupplier;
    }

    /**
     * This will handle passing {@link Intent} results back to the {@link WindowAndroid}.  It will
     * return whether or not the {@link WindowAndroid} has consumed the event or not.
     */
    @CallSuper
    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (mIntentRequestTracker.onActivityResult(requestCode, resultCode, intent)) {
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
        mIntentRequestTracker.saveInstanceState(outState);

        mLifecycleDispatcher.dispatchOnSaveInstanceState(outState);
    }

    @CallSuper
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        mLifecycleDispatcher.dispatchOnWindowFocusChanged(hasFocus);
    }

    @CallSuper
    @Override
    public void recreate() {
        super.recreate();

        mLifecycleDispatcher.dispatchOnRecreate();

        // TODO(crbug.com/40793204): Remove stack trace logging once root cause of bug is
        // identified & fixed.
        // Piggybacking for multi-instance bug crbug.com/1484026.
        Log.i(TAG_MULTI_INSTANCE, "Tracing recreate().");
        Thread.dumpStack();
    }

    @CallSuper
    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);

        mLifecycleDispatcher.dispatchOnTopResumedActivityChanged(isTopResumedActivity);
        mNativeInitializationController.onTopResumedActivityChanged(isTopResumedActivity);
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
    protected void onOrientationChange(int orientation) {}

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

    /** Returns whether initial inflation is complete. */
    public boolean isInitialLayoutInflationComplete() {
        return mInitialLayoutInflationComplete;
    }

    /**
     * @return {@link ActivityLifecycleDispatcher} associated with this activity.
     */
    @Override
    public ActivityLifecycleDispatcher getLifecycleDispatcher() {
        return mLifecycleDispatcher;
    }

    /**
     * @return {@link MultiWindowModeStateDispatcher} associated with this activity.
     */
    public MultiWindowModeStateDispatcher getMultiWindowModeStateDispatcher() {
        return mMultiWindowModeStateDispatcher;
    }

    @Override
    public boolean moveTaskToBack(boolean nonRoot) {
        // On Android (at least until N) moving the task to the background flakily stops the
        // Activity from being finished, breaking tests. Trying to bring the task back to the
        // foreground after also happens to be flaky, so just allow tests to prevent actually moving
        // to the background.
        if (sInterceptMoveTaskToBackForTesting) {
            sBackInterceptedForTesting = true;
            return false;
        } else if (BuildConfig.IS_FOR_TEST) {
            assert false
                    : "moveTaskToBack must be intercepted or it will create flaky tests. "
                            + "See #interceptMoveTaskToBackForTesting";
        }
        return super.moveTaskToBack(nonRoot);
    }

    public static void interceptMoveTaskToBackForTesting() {
        sInterceptMoveTaskToBackForTesting = true;
        sBackInterceptedForTesting = false;
        ResettersForTesting.register(() -> sInterceptMoveTaskToBackForTesting = false);
    }

    public static boolean wasMoveTaskToBackInterceptedForTesting() {
        assert sInterceptMoveTaskToBackForTesting;
        return sBackInterceptedForTesting;
    }

    private boolean shouldDisableHardwareAcceleration() {
        // Low end devices should disable hardware acceleration for memory gains.
        return SysUtils.isLowEndDevice();
    }

    protected void enableHardwareAcceleration() {
        // HW acceleration is disabled in the manifest and may be re-enabled here.
        if (!shouldDisableHardwareAcceleration()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);

            // When HW acceleration is enabled manually for an activity, child windows (e.g.
            // dialogs) don't inherit HW acceleration state. However, when HW acceleration is
            // enabled in the manifest, child windows do inherit HW acceleration state. That
            // looks like a bug, so I filed b/23036374
            //
            // In the meanwhile the workaround is to call
            //   window.setWindowManager(..., hardwareAccelerated=true)
            // to let the window know that it's HW accelerated. However, since there is no way
            // to know 'appToken' argument until window's view is attached to the window (!!),
            // we have to do the workaround in onAttachedToWindow()
            mSetWindowHWA = true;
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // See enableHardwareAcceleration()
        if (mSetWindowHWA) {
            mSetWindowHWA = false;
            getWindow()
                    .setWindowManager(
                            getWindow().getWindowManager(),
                            getWindow().getAttributes().token,
                            getComponentName().flattenToString(),
                            /* hardwareAccelerated= */ true);
        }
    }
}
