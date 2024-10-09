// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.util.Rational;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

/** A controller for entering Picture in Picture mode with fullscreen videos. */
public class FullscreenVideoPictureInPictureController {
    private static final String TAG = "VideoPersist";
    private static final int AUTO_PIP_UPDATE_DELAY = 500 /* msec */;

    // Metrics

    private @interface MetricsAttemptResult {
        static final int SUCCESS = 0;
        static final int NO_SYSTEM_SUPPORT = 1;
        static final int NO_FEATURE = 2;
        // Obsolete: static final int NO_ACTIVITY_SUPPORT = 3;
        static final int ALREADY_RUNNING = 4;
        static final int RESTARTING = 5;
        static final int FINISHING = 6;
        static final int NO_WEB_CONTENTS = 7;
        static final int NO_VIDEO = 8;
        static final int APP_TASKS = 9;
    }

    private @interface MetricsEndReason {
        static final int RESUME = 0;
        // Obsolete: NAVIGATION = 1;
        static final int CLOSE = 2;
        static final int CRASH = 3;
        static final int NEW_TAB = 4;
        static final int REPARENT = 5;
        static final int LEFT_FULLSCREEN = 6;
        static final int WEB_CONTENTS_LEFT_FULLSCREEN = 7;
    }

    private static final float MIN_ASPECT_RATIO = 1 / 2.39f;
    private static final float MAX_ASPECT_RATIO = 2.39f;

    // Somewhat arbitrarily-chosen minimum interval between when we're notified that we have entered
    // Picture in Picture, and when we'll try to exit it.  Otherwise, Android can get into a bad
    // state when chrome is broght to the foreground again -- it still is clipped to a pip-sized
    // area, complete with rounded corners.  See https://crbug.com/1421703 for more details.
    /* package */ static final long MIN_EXIT_DELAY_MILLIS = 50;

    // TODO(crbug.com/40853653): Auto-enter seems to be causing a very bad
    // display issue on S (31 or 32), so turn this off for S.
    private static final boolean ENABLE_AUTO_ENTER =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU;

    // Components names that won't trigger the pip mode. Use cases like notification clicks won't
    // trigger pip.
    private static final Set<String> NO_PIP_COMPONENT_NAMES =
            Collections.singleton(NotificationIntentInterceptor.TrampolineActivity.class.getName());

    // If true, then we will use `setSourceRectHint()` to enable fancy transitions into pip.
    // However, since this also causes visible flicker especially when transitioning from landscape
    // to portrait, this is off by default.
    private static final boolean sUseSourceRectHint = false;

    /** Callbacks to cleanup after leaving PiP. */
    private final List<Runnable> mOnLeavePipCallbacks = new LinkedList<>();

    /** Current observers, if any. */
    @Nullable DismissActivityOnTabChangeObserver mActivityTabObserver;

    @Nullable FullscreenManager.Observer mFullscreenListener;

    private final Activity mActivity;
    private final ActivityTabProvider mActivityTabProvider;
    private final FullscreenManager mFullscreenManager;

    /** Did we last tell the framework that auto-enter is allowed (true) or not? */
    private boolean mIsAutoEnterAllowed;

    /** Wall clock time when we last entered Picture in Picture */
    private long mLastOnEnteredTimeMillis;

    /** Runnable that will update our autopip config. */
    private Runnable mUpdateAutoPipRunnable = this::updateAutoPictureInPictureStatusIfNeeded;

    /** Do we believe that media is currently playing or not? */
    private boolean mIsPlaying;

    /** Is media paused because we suspended it when the pip window was stashed? */
    private boolean mIsSuspendedForStash;

    public FullscreenVideoPictureInPictureController(
            Activity activity,
            ActivityTabProvider activityTabProvider,
            FullscreenManager fullscreenManager) {
        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mFullscreenManager = fullscreenManager;

        if (ENABLE_AUTO_ENTER) {
            addObserversIfNeeded();
        }
    }

    /** Convenience method to get the {@link WebContents} from the active Tab. */
    private @Nullable WebContents getWebContents() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null) return null;
        return tab.getWebContents();
    }

    private static void recordAttemptResult(@MetricsAttemptResult int result) {
        // Silently ignore NO_VIDEO, since it's spammy.
        if (result == MetricsAttemptResult.NO_VIDEO) return;
    }

    /**
     * Return a `MetricsAttemptResult` for whether Picture in Picture is okay or not.
     *
     * @param checkCurrentMode should be true if and only if "already in PiP mode" is sufficient to
     *     cause this to return failure.
     */
    private @MetricsAttemptResult int getAttemptResult(boolean checkCurrentMode) {
        WebContents webContents = getWebContents();
        if (webContents == null) {
            return MetricsAttemptResult.NO_WEB_CONTENTS;
        }

        assertLibraryLoaderIsInitialized();

        // Only auto-PiP if there is a playing fullscreen video that allows PiP.
        if (!webContents.hasActiveEffectivelyFullscreenVideo()
                || !webContents.isPictureInPictureAllowedForFullscreenVideo()) {
            return MetricsAttemptResult.NO_VIDEO;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return MetricsAttemptResult.NO_SYSTEM_SUPPORT;
        }

        if (!mActivity
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            Log.d(TAG, "Activity does not have PiP feature.");
            return MetricsAttemptResult.NO_FEATURE;
        }

        // Don't PiP if we are already in PiP.
        if (checkCurrentMode && mActivity.isInPictureInPictureMode()) {
            Log.d(TAG, "Activity is already in PiP.");
            return MetricsAttemptResult.ALREADY_RUNNING;
        }

        // This means the activity is going to be restarted, so don't PiP.
        if (mActivity.isChangingConfigurations()) {
            Log.d(TAG, "Activity is being restarted.");
            return MetricsAttemptResult.RESTARTING;
        }

        // Don't PiP if the activity is finishing.
        if (mActivity.isFinishing()) {
            Log.d(TAG, "Activity is finishing.");
            return MetricsAttemptResult.FINISHING;
        }

        // Don't trigger pip mode for certain types of usage, like notification click.
        if (!canStartPipBasedOnRecentTasks()) {
            Log.d(TAG, "Block pip due to recent app tasks.");
            return MetricsAttemptResult.APP_TASKS;
        }

        return MetricsAttemptResult.SUCCESS;
    }

    private boolean canStartPipBasedOnRecentTasks() {
        return AndroidTaskUtils.getRecentAppTasksMatchingComponentNames(
                        mActivity, NO_PIP_COMPONENT_NAMES)
                .isEmpty();
    }

    /**
     * Return a `MetricsAttemptResult` for whether Picture in Picture is okay or not. Considers that
     * "already in PiP mode" is a reason to say no.
     */
    private @MetricsAttemptResult int getAttemptResult() {
        return getAttemptResult(true);
    }

    /**
     * Attempt to enter Picture in Picture mode if there is fullscreen video.  If Picture in Picture
     * is not applicable, then do nothing.  It is still the caller's responsibility to notify us if
     * Picture in Picture mode is started; at most, we will request it from the framework.
     */
    public void attemptPictureInPicture() {
        // If there are already callbacks registered, then do nothing.
        final @MetricsAttemptResult int result = getAttemptResult();
        Log.i(TAG, "Attempted picture-in-picture with result: " + result);

        recordAttemptResult(result);
        if (result != MetricsAttemptResult.SUCCESS) return;

        final WebContents webContents = getWebContents();
        assert webContents != null;

        Rect bounds = getVideoBounds(webContents, mActivity);
        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        if (bounds != null) {
            if (sUseSourceRectHint) {
                builder.setAspectRatio(new Rational(bounds.width(), bounds.height()));
                builder.setSourceRectHint(bounds);
            }
        }

        try {
            if (!mActivity.enterPictureInPictureMode(builder.build())) return;
        } catch (IllegalStateException | IllegalArgumentException e) {
            Log.e(TAG, "Error entering PiP with bounds %s", bounds, e);
            return;
        }
    }

    /**
     * Called to update whether the pip window is stashed offscreen temporarily. This is not
     * guaranteed to be called only for changes, since Android doesn't guarantee that. In fact,
     * Android does call multiple times in practice.
     *
     * <p>Also note that, when interacting with the media notification, sometimes Android will
     * un-stash a pip window without telling us. It'll also often skip telling us about the next
     * time it's stashed. So, we have to be fairly robust against getting out of sync.
     *
     * @param stashed whether the pip window is, or is not, currently stashed offscreen.
     */
    public void onStashReported(boolean stashed) {
        final MediaSession mediaSession = getMediaSession();
        if (mediaSession == null) {
            return;
        }

        // Note that these will do nothing if called multiple times in a row with the same value for
        // `stashed`.  Also note that `mIsSuspendedForStash` can be reset elsewhere, to help to
        // account for missed events from Android.
        if (mIsPlaying && stashed && !mIsSuspendedForStash) {
            mediaSession.suspend();
            mIsSuspendedForStash = true;
        } else if (!mIsPlaying && !stashed && mIsSuspendedForStash) {
            // Don't resume if we didn't pause it on the transition into stash.  For example, don't
            // start playing media that was already paused when the user stashed the pip window.
            mediaSession.resume();
            mIsSuspendedForStash = false;
        }
    }

    /**
     * Notify us that Picture in Picture mode has started. This can be because we requested it in
     * {@link #attemptPictureInPicture()} or because we auto-entered Picture in Picture.
     */
    public void onEnteredPictureInPictureMode() {
        Log.i(TAG, "Entered Picture-in-picture.");

        mLastOnEnteredTimeMillis = SystemClock.elapsedRealtime();

        // Inform the WebContents when we enter and when we leave PiP.
        final WebContents webContents = getWebContents();
        // If we're closing the tab, just stop here.
        if (webContents == null) {
            Log.i(TAG, "Tab is closing, not entering Picture-in-picture");
            return;
        }

        webContents.setHasPersistentVideo(true);

        final Tab activityTab = mActivityTabProvider.get();

        // We don't want InfoBars displaying while in PiP, they cover too much content.
        getInfoBarContainerForTab(activityTab).setHidden(true);

        mOnLeavePipCallbacks.add(
                () -> {
                    Log.i(TAG, "Running Picture-in-picture exit callbacks");
                    webContents.setHasPersistentVideo(false);
                    getInfoBarContainerForTab(activityTab).setHidden(false);
                });

        // Setup observers to dismiss the Activity on events that should end PiP.  In auto-enter
        // mode, these might be registered already.
        addObserversIfNeeded();
    }

    /**
     * Called when `mActivity` is stopped, to allow us to clean up. A new instance will be created
     * later, when the activity is restarted.
     */
    public void onStop() {
        // Unconditionally remove listeners, since a new instance will be created onStart.
        removeObserversIfNeeded();
    }

    private static Rect getVideoBounds(WebContents webContents, Activity activity) {
        Rect rect = webContents.getFullscreenVideoSize();
        if (rect == null || rect.width() == 0 || rect.height() == 0) return null;

        float videoAspectRatio =
                MathUtils.clamp(
                        rect.width() / (float) rect.height(), MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);

        int windowWidth = activity.getWindow().getDecorView().getWidth();
        int windowHeight = activity.getWindow().getDecorView().getHeight();
        float phoneAspectRatio = windowWidth / (float) windowHeight;

        // The currently playing video size is the video frame size, not the on-screen size.
        // We know the video will be touching either the sides or the top and bottom of the screen
        // so we can work out the screen bounds of the video from this.
        int width;
        int height;
        if (videoAspectRatio > phoneAspectRatio) {
            // The video takes up the full width of the phone and there are black bars at the top
            // and bottom.
            width = windowWidth;
            height = (int) (windowWidth / videoAspectRatio);
        } else {
            // The video takes up the full height of the phone and there are black bars at the
            // sides.
            width = (int) (windowHeight * videoAspectRatio);
            height = windowHeight;
        }

        // In the if above, either |height == windowHeight| or |width == windowWidth| so one of
        // left or top will be zero.
        int left = (windowWidth - width) / 2;
        int top = (windowHeight - height) / 2;
        return new Rect(left, top, left + width, top + height);
    }

    /**
     * Notify us that the framework has exited from Picture in Picture mode. Perform any cleanup
     * required. It's okay to call this even when we don't believe that we're in PiP mode.
     */
    public void onFrameworkExitedPictureInPicture() {
        Log.i(TAG, "Framework exited picture in picture");
        onExitedPictureInPicture(MetricsEndReason.RESUME);
    }

    /**
     * Called when we have exited Picture in Picture mode, to switch the browser back into non-
     * PiP mode.  If we registered observers due to a non-auto PiP, then also unregister the
     * observers as well.
     *
     * It's okay if we're not currently in Picture in Picture mode.
     */
    private void onExitedPictureInPicture(@MetricsEndReason int reason) {
        Log.i(TAG, "Exited picture in picture with reason: " + reason);

        // If we don't believe that a Picture in Picture session is active, it means that the
        // cleanup call happened while Chrome was not PIP'ing. The early return also avoid recording
        // the reason why the (non-)PIP session ended.
        if (!isPipSessionActive()) return;

        // This method can be called when we haven't been PiPed. We use Callbacks to ensure we only
        // do cleanup if it is required.
        for (Runnable callback : mOnLeavePipCallbacks) {
            callback.run();
        }
        mOnLeavePipCallbacks.clear();

        // Leave the callbacks in place if we're using them to decide about auto-pip.  Otherwise,
        // they're only registered to detect when we leave.  They will be re-added if we're told to
        // re-enter pip later.
        if (!ENABLE_AUTO_ENTER) {
            removeObserversIfNeeded();
        }
    }

    /**
     * Add observers for the tab / activity / etc. state, so that we can tell if Picture in Picture
     * is allowed or not.
     */
    private void addObserversIfNeeded() {
        if (mActivityTabObserver == null) {
            // This will auto-register itself.
            mActivityTabObserver = new DismissActivityOnTabChangeObserver(mActivity);
        }

        if (mFullscreenListener == null) {
            mFullscreenListener =
                    new FullscreenManager.Observer() {
                        @Override
                        public void onExitFullscreen(Tab tab) {
                            Log.i(TAG, "Exiting fullscreen");
                            dismissActivityIfNeeded(mActivity, MetricsEndReason.LEFT_FULLSCREEN);
                        }
                    };

            mFullscreenManager.addObserver(mFullscreenListener);
        }
    }

    /** Unregister callbacks, or do nothing successfully if there aren't any. */
    private void removeObserversIfNeeded() {
        if (mFullscreenListener != null) {
            mFullscreenManager.removeObserver(mFullscreenListener);
            mFullscreenListener = null;
        }

        if (mActivityTabObserver != null) {
            mActivityTabObserver.cleanup();
            mActivityTabObserver = null;
        }
    }

    /**
     * Notify Android if it's okay to auto-enter Picture in Picture mode.
     *
     * <p>Suppress `PictureInPictureIssue` since it doesn't like that the call to
     * `SetSourceRectHint()` is conditional even when we call `setAutoEnterEnabled()`. It's correct
     * as-is, since we don't want the source rect transition when we don't have a proper source
     * rect. It would end up animating the wrong part of the viewport.
     */
    @SuppressLint("PictureInPictureIssue")
    private void updateAutoPictureInPictureStatusIfNeeded() {
        // Do nothing if Android doesn't support auto-enter.
        if (!ENABLE_AUTO_ENTER) {
            return;
        }

        // Do not check if we're in PiP mode or not, since we're called during transitions into and
        // out of it.  The framework won't try to auto-enter if we're already there anyway.
        final boolean allowed = (getAttemptResult(false) == MetricsAttemptResult.SUCCESS);
        if (!allowed && !mIsAutoEnterAllowed) {
            // Don't notify the framework if we were not and continue to be not allowed.  In the
            // case where we're allowed, the bounds for the source rect can change even if we were
            // allowed before.
            return;
        }

        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        if (allowed) {
            final WebContents webContents = getWebContents();
            assert webContents != null;

            final Rect bounds = getVideoBounds(webContents, mActivity);
            if (bounds != null) {
                if (sUseSourceRectHint) {
                    builder.setAspectRatio(new Rational(bounds.width(), bounds.height()));
                    builder.setSourceRectHint(bounds);
                }
            }
        }
        builder.setAutoEnterEnabled(allowed);

        mIsAutoEnterAllowed = allowed;
        try {
            mActivity.setPictureInPictureParams(builder.build());
        } catch (Exception e) {
            Log.e(TAG, "Error setting PiP params", e);
        }
    }

    /**
     * Return whether or not we believe that we're in Picture in Picture mode.  This might not agree
     * with what the framework thinks, which can change asynchronously with respect to what we do
     * here.  This only indicates whether we've {@link onEnteredPictureInPictureMode} without later
     * exiting it.
     */
    private boolean isPipSessionActive() {
        return !mOnLeavePipCallbacks.isEmpty();
    }

    /**
     * Moves the Activity to the back if we're in Picture in Picture mode, and performs any
     * cleanup of our internal state, due to something that should cancel Picture in Picture mode.
     * It's okay if we're not in Picture in Picture mode; we'll reset our internal state as needed.
     *
     * This will also update our auto-PiP preference with the framework, if it's changed.
     */
    private void dismissActivityIfNeeded(Activity activity, @MetricsEndReason int reason) {
        // Something interesting happened -- make sure we've updated our preference for auto-PiP
        // with the framework, if applicable.
        Log.i(TAG, "Dismiss activity with reason " + reason);
        updateAutoPictureInPictureStatusIfNeeded();

        if (!isPipSessionActive()) {
            return;
        }

        // If we just entered PiP, then re-post this.  There are corner cases where we exit PiP via
        // some other way, then re-enter it that might go wrong if we don't cancel this, but all of
        // these cases are very questionable.  The important thing is that, once
        // `onExitedPictureInPicture` runs, future callbacks will either (a) do nothing because
        // `isPipSessionActive()` will be false, or (b) try to close PiP properly.  We also don't
        // try to pro-rate the exit delay; it's short and arbitrary anyway.
        if (SystemClock.elapsedRealtime() - mLastOnEnteredTimeMillis < MIN_EXIT_DELAY_MILLIS) {
            Log.i(TAG, "Posting deferred callback to dismiss activity.");
            PostTask.postDelayedTask(
                    TaskTraits.UI_USER_BLOCKING,
                    () -> dismissActivityIfNeeded(activity, reason),
                    MIN_EXIT_DELAY_MILLIS);
            return;
        }

        // If we're currently in Picture in Picture mode, then notify the framework to exit it.
        activity.moveTaskToBack(true);
        onExitedPictureInPicture(reason);
    }

    /**
     * A class to dismiss the Activity when the tab:
     * - Closes.
     * - Re-parents (attaches to a different activity).
     * - Crashes.
     * - Leaves fullscreen.
     */
    private class DismissActivityOnTabEventObserver extends EmptyTabObserver {
        private final Activity mActivity;
        private final Tab mTab;
        private WebContents mWebContents;
        private DismissActivityOnWebContentsObserver mWebContentsObserver;

        public DismissActivityOnTabEventObserver(Activity activity, Tab tab) {
            mActivity = activity;
            mTab = tab;
            mTab.addObserver(this);
            registerWebContentsObserver();
        }

        // Register an observer if we have a WebContents.
        private void registerWebContentsObserver() {
            if (mTab == null) return;
            mWebContents = mTab.getWebContents();
            if (mWebContents == null) return;
            mWebContentsObserver =
                    new DismissActivityOnWebContentsObserver(mActivity, mWebContents);
        }

        /** Clean up everything, and unregister `this`. */
        public void cleanup() {
            cleanupWebContentsObserver();
            if (mTab != null) {
                mTab.removeObserver(this);
            }
        }

        private void cleanupWebContentsObserver() {
            if (mWebContentsObserver == null) return;
            mWebContentsObserver.cleanup();
            mWebContentsObserver = null;
            mWebContents = null;
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
            if (window != null) dismissActivityIfNeeded(mActivity, MetricsEndReason.REPARENT);
        }

        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            dismissActivityIfNeeded(mActivity, MetricsEndReason.CLOSE);
            cleanupWebContentsObserver();
        }

        @Override
        public void onCrash(Tab tab) {
            dismissActivityIfNeeded(mActivity, MetricsEndReason.CRASH);
            cleanupWebContentsObserver();
        }

        @Override
        public void webContentsWillSwap(Tab tab) {
            dismissActivityIfNeeded(mActivity, MetricsEndReason.WEB_CONTENTS_LEFT_FULLSCREEN);
            cleanupWebContentsObserver();
        }

        @Override
        public void onContentChanged(Tab tab) {
            if (tab != mTab) return;
            // While webContentsWillSwap() probably did this, doesn't hurt to do it again.
            cleanupWebContentsObserver();
            // Now that we have a new WebContents, start listening.
            registerWebContentsObserver();
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab != mTab) return;
            cleanupWebContentsObserver();
            // Don't bother to clean up here -- it clears the observers anyway,
            // and TabChangeObserver will do it anyway.
        }
    }

    /** A class to dismiss the Activity when the tab changes. */
    private class DismissActivityOnTabChangeObserver implements Callback<Tab> {
        private final Activity mActivity;
        private Tab mCurrentTab;
        private DismissActivityOnTabEventObserver mTabEventObserver;

        private DismissActivityOnTabChangeObserver(Activity activity) {
            mActivity = activity;
            mCurrentTab = mActivityTabProvider.get();
            mActivityTabProvider.addObserver(this);
            registerTabEventObserver();
        }

        private void registerTabEventObserver() {
            if (mCurrentTab == null) return;

            mTabEventObserver = new DismissActivityOnTabEventObserver(mActivity, mCurrentTab);
        }

        /** Called to clean up everything, and unregister `this`. */
        public void cleanup() {
            if (mTabEventObserver != null) {
                mTabEventObserver.cleanup();
                mTabEventObserver = null;
            }
            mCurrentTab = null;
            mActivityTabProvider.removeObserver(this);
        }

        @Override
        public void onResult(Tab tab) {
            if (mCurrentTab == tab) return;

            // If we're switching tabs, including to the case of "no tab", then get rid of the
            // observer on the current tab.
            if (mTabEventObserver != null) {
                mTabEventObserver.cleanup();
                mTabEventObserver = null;
            }

            mCurrentTab = tab;

            dismissActivityIfNeeded(mActivity, MetricsEndReason.NEW_TAB);

            // If we have an incoming tab, then register a tab event observer on it.  Note that if
            // cleanup() was called during the `dismissActivityIfNeeded` call, then this will be
            // skipped.
            if (mCurrentTab != null) {
                registerTabEventObserver();
            }

            updateAutoPictureInPictureStatusIfNeeded();
        }
    }

    /**
     * A class to dismiss the Activity when the Web Contents stops being effectively fullscreen.
     * This catches page navigations but unlike TabObserver's onNavigationFinished will not trigger
     * if an iframe without the fullscreen video navigates.
     */
    private class DismissActivityOnWebContentsObserver extends WebContentsObserver {
        private final Activity mActivity;
        private final WebContents mWebContents;

        public DismissActivityOnWebContentsObserver(Activity activity, WebContents webContents) {
            mActivity = activity;
            mWebContents = webContents;
            mWebContents.addObserver(this);
        }

        /** Unregister us from `mWebContents`. */
        public void cleanup() {
            mWebContents.removeObserver(this);
        }

        @Override
        public void mediaStartedPlaying() {
            // We have no idea if the effectively fullscreen video started playing, but this will
            // check if we have an active one.
            updateAutoPictureInPictureStatusIfNeeded();
            mIsPlaying = true;
            // If we were suspended for stash, forget because something else caused us to start
            // playing while stashed.  For example, the user might press play in the notification.
            mIsSuspendedForStash = false;
        }

        @Override
        public void mediaStoppedPlaying() {
            // As above, we don't know if it was the effectively fullscreen video that stopped. Even
            // if it is, note that this won't cause us to exit Picture in Picture mode if we're in
            // it.
            updateAutoPictureInPictureStatusIfNeeded();
            mIsPlaying = false;
        }

        @Override
        public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
            Log.i(TAG, "Effective video fullscreen change: " + isFullscreen);
            if (isFullscreen) {
                updateAutoPictureInPictureStatusIfNeeded();
                // Also post a delayed handler to update the status again, once things have had some
                // time to settle.  When switching into landscape mode, for example, sometimes we're
                // called before relayout has happened, causing the source rectangle for the pip
                // transition to be wrong.  This causes the pip window to look like it moves to the
                // wrong part of the screen / partially clipped before snapping to its normal place.
                PostTask.postDelayedTask(
                        TaskTraits.UI_BEST_EFFORT, mUpdateAutoPipRunnable, AUTO_PIP_UPDATE_DELAY);
            } else {
                dismissActivityIfNeeded(mActivity, MetricsEndReason.WEB_CONTENTS_LEFT_FULLSCREEN);
            }
        }
    }

    /** Protected to allow tests to override, since mocking statics is error-prone. */
    @VisibleForTesting
    /* package */ InfoBarContainer getInfoBarContainerForTab(Tab tab) {
        return InfoBarContainer.get(tab);
    }

    /**
     * Protected to allow tests to override, since it breaks in N.  It's also not clear that we
     * need this at all.
     */
    @VisibleForTesting
    /* package */ void assertLibraryLoaderIsInitialized() {
        // Non-null WebContents implies the native library has been loaded.
        assert LibraryLoader.getInstance().isInitialized();
    }

    /**
     * Return the {@link MediaSession}, if any, for our WebContents. Used for testing to get around
     * MediaSession's static getter.
     */
    @VisibleForTesting
    /* package */ @Nullable
    MediaSession getMediaSession() {
        // This works if `getWebContents()` is null.
        return MediaSession.fromWebContents(getWebContents());
    }
}
