// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.util.Rational;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.annotations.VerifiesOnO;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.util.LinkedList;
import java.util.List;

/**
 * A controller for entering Android O Picture in Picture mode with fullscreen videos.
 *
 * Do not inline to prevent class verification errors on pre-O runtimes.
 */
@VerifiesOnO
@TargetApi(Build.VERSION_CODES.O)
public class PictureInPictureController {
    private static final String TAG = "VideoPersist";

    // Metrics
    private static final String METRICS_DURATION = "Media.VideoPersistence.Duration";

    private static final String METRICS_ATTEMPT_RESULT = "Media.VideoPersistence.AttemptResult";
    @IntDef({
            MetricsAttemptResult.SUCCESS,
            MetricsAttemptResult.NO_SYSTEM_SUPPORT,
            MetricsAttemptResult.NO_FEATURE,
            MetricsAttemptResult.ALREADY_RUNNING,
            MetricsAttemptResult.RESTARTING,
            MetricsAttemptResult.FINISHING,
            MetricsAttemptResult.NO_WEB_CONTENTS,
            MetricsAttemptResult.NO_VIDEO,
    })
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
    }
    // For UMA, not a valid MetricsAttemptResult.
    private static final int METRICS_ATTEMPT_RESULT_COUNT = 9;

    private static final String METRICS_END_REASON = "Media.VideoPersistence.EndReason";
    @IntDef({
            MetricsEndReason.RESUME,
            MetricsEndReason.CLOSE,
            MetricsEndReason.CRASH,
            MetricsEndReason.NEW_TAB,
            MetricsEndReason.REPARENT,
            MetricsEndReason.LEFT_FULLSCREEN,
            MetricsEndReason.WEB_CONTENTS_LEFT_FULLSCREEN,
    })
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
    private static final int METRICS_END_REASON_COUNT = 8;

    private static final float MIN_ASPECT_RATIO = 1 / 2.39f;
    private static final float MAX_ASPECT_RATIO = 2.39f;

    /** Callbacks to cleanup after leaving PiP. */
    private final List<Runnable> mOnLeavePipCallbacks = new LinkedList<>();
    /** Current observers, if any. */
    @Nullable
    DismissActivityOnTabChangeObserver mActivityTabObserver;
    @Nullable
    FullscreenManager.Observer mFullscreenListener;

    private final Activity mActivity;
    private final ActivityTabProvider mActivityTabProvider;
    private final FullscreenManager mFullscreenManager;

    public PictureInPictureController(Activity activity, ActivityTabProvider activityTabProvider,
            FullscreenManager fullscreenManager) {
        mActivity = activity;
        mActivityTabProvider = activityTabProvider;
        mFullscreenManager = fullscreenManager;
    }

    /**
     * Convenience method to get the {@link WebContents} from the active Tab.
     */
    @Nullable
    private WebContents getWebContents() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null) return null;
        return tab.getWebContents();
    }

    private static void recordAttemptResult(@MetricsAttemptResult int result) {
        // Silently ignore NO_VIDEO, since it's spammy.
        if (result == MetricsAttemptResult.NO_VIDEO) return;

        RecordHistogram.recordEnumeratedHistogram(
                METRICS_ATTEMPT_RESULT, result, METRICS_ATTEMPT_RESULT_COUNT);
    }

    /**
     * Return a METRICS_ATTEMPT_REASON_* for whether Picture in Picture is okay or not.
     * @param checkCurrentMode should be true if and only if "already in PiP mode" is sufficient to
     *                         cause this to return failure.
     */
    @MetricsAttemptResult
    private int getAttemptResult(boolean checkCurrentMode) {
        WebContents webContents = getWebContents();
        if (webContents == null) {
            return MetricsAttemptResult.NO_WEB_CONTENTS;
        }

        // Non-null WebContents implies the native library has been loaded.
        assert LibraryLoader.getInstance().isInitialized();

        // Only auto-PiP if there is a playing fullscreen video that allows PiP.
        if (!webContents.hasActiveEffectivelyFullscreenVideo()
                || !webContents.isPictureInPictureAllowedForFullscreenVideo()) {
            return MetricsAttemptResult.NO_VIDEO;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return MetricsAttemptResult.NO_SYSTEM_SUPPORT;
        }

        if (!mActivity.getPackageManager().hasSystemFeature(
                    PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
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

        return MetricsAttemptResult.SUCCESS;
    }

    /**
     * Return a METRICS_ATTEMPT_REASON_* for whether Picture in Picture is okay or not.  Considers
     * that "already in PiP mode" is a reason to say no.
     */
    @MetricsAttemptResult
    private int getAttemptResult() {
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
        recordAttemptResult(result);
        if (result != MetricsAttemptResult.SUCCESS) return;

        final WebContents webContents = getWebContents();
        assert webContents != null;

        Rect bounds = getVideoBounds(webContents, mActivity);
        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        if (bounds != null) {
            builder.setAspectRatio(new Rational(bounds.width(), bounds.height()));
            builder.setSourceRectHint(bounds);
        }

        try {
            if (!mActivity.enterPictureInPictureMode(builder.build())) return;
        } catch (IllegalStateException | IllegalArgumentException e) {
            Log.e(TAG, "Error entering PiP with bounds (%d, %d): %s",
                    bounds.width(), bounds.height(), e);
            return;
        }
    }

    /**
     * Notify us that Picture in Picture mode has started.  This can be because we requested it in
     * {@link #attemptPictureInPicture()}.
     */
    public void onEnteredPictureInPictureMode() {
        // Inform the WebContents when we enter and when we leave PiP.
        final WebContents webContents = getWebContents();
        assert webContents != null;

        webContents.setHasPersistentVideo(true);

        final Tab activityTab = mActivityTabProvider.get();

        // We don't want InfoBars displaying while in PiP, they cover too much content.
        InfoBarContainer.get(activityTab).setHidden(true);

        mOnLeavePipCallbacks.add(() -> {
            webContents.setHasPersistentVideo(false);
            InfoBarContainer.get(activityTab).setHidden(false);
        });

        // Setup observers to dismiss the Activity on events that should end PiP.
        addObserversIfNeeded();

        long startTimeMs = SystemClock.elapsedRealtime();
        mOnLeavePipCallbacks.add(() -> {
            long pipTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
            RecordHistogram.recordCustomTimesHistogram(METRICS_DURATION, pipTimeMs,
                    DateUtils.SECOND_IN_MILLIS * 7, DateUtils.HOUR_IN_MILLIS * 10, 50);
        });
    }

    private static Rect getVideoBounds(WebContents webContents, Activity activity) {
        Rect rect = webContents.getFullscreenVideoSize();
        if (rect == null || rect.width() == 0 || rect.height() == 0) return null;

        float videoAspectRatio = MathUtils.clamp(rect.width() / (float) rect.height(),
                MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);

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
     * If we have previously entered Picture in Picture, perform cleanup.
     */
    public void cleanup() {
        exitPictureInPicture(MetricsEndReason.RESUME);
    }

    /**
     * Switch out of Picture in Picture mode.
     */
    private void exitPictureInPicture(@MetricsEndReason int reason) {
        // If `mOnLeavePipCallbacks` is empty, it means that the cleanup call happened while Chrome
        // was not PIP'ing. The early return also avoid recording the reason why the PIP session
        // ended.
        if (mOnLeavePipCallbacks.isEmpty()) return;

        // This method can be called when we haven't been PiPed. We use Callbacks to ensure we only
        // do cleanup if it is required.
        for (Runnable callback : mOnLeavePipCallbacks) {
            callback.run();
        }
        mOnLeavePipCallbacks.clear();

        removeObserversIfNeeded();

        RecordHistogram.recordEnumeratedHistogram(
                METRICS_END_REASON, reason, METRICS_END_REASON_COUNT);
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
            mFullscreenListener = new FullscreenManager.Observer() {
                @Override
                public void onExitFullscreen(Tab tab) {
                    dismissActivity(mActivity, MetricsEndReason.LEFT_FULLSCREEN);
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

    /** Moves the Activity to the back and performs all cleanup. */
    private void dismissActivity(Activity activity, @MetricsEndReason int reason) {
        activity.moveTaskToBack(true);
        exitPictureInPicture(reason);
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

        /**
         * Clean up everything, and unregister `this`.
         */
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
            if (window != null) dismissActivity(mActivity, MetricsEndReason.REPARENT);
        }

        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            dismissActivity(mActivity, MetricsEndReason.CLOSE);
            cleanupWebContentsObserver();
        }

        @Override
        public void onCrash(Tab tab) {
            dismissActivity(mActivity, MetricsEndReason.CRASH);
            cleanupWebContentsObserver();
        }

        @Override
        public void webContentsWillSwap(Tab tab) {
            dismissActivity(mActivity, MetricsEndReason.WEB_CONTENTS_LEFT_FULLSCREEN);
            cleanupWebContentsObserver();
        }

        @Override
        public void onContentChanged(Tab tab) {
            if (tab == mTab) return;
            // While webContentsWillSwap() probably did this, doesn't hurt to do it again.
            cleanupWebContentsObserver();
            // Now that we have a new WebContents, start listening.
            registerWebContentsObserver();
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

        /**
         * Called to clean up everything, and unregister `this`.
         */
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

            dismissActivity(mActivity, MetricsEndReason.NEW_TAB);
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

        /**
         * Unregister us from `mWebContents`.
         */
        public void cleanup() {
            mWebContents.removeObserver(this);
        }

        @Override
        public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
            if (isFullscreen) return;
            dismissActivity(mActivity, MetricsEndReason.WEB_CONTENTS_LEFT_FULLSCREEN);
        }
    }
}
