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

import androidx.annotation.Nullable;

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
import org.chromium.chrome.browser.tab.TabObserver;
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
    private static final int METRICS_ATTEMPT_RESULT_SUCCESS = 0;
    private static final int METRICS_ATTEMPT_RESULT_NO_SYSTEM_SUPPORT = 1;
    private static final int METRICS_ATTEMPT_RESULT_NO_FEATURE = 2;
    // Obsolete: private static final int METRICS_ATTEMPT_RESULT_NO_ACTIVITY_SUPPORT = 3;
    private static final int METRICS_ATTEMPT_RESULT_ALREADY_RUNNING = 4;
    private static final int METRICS_ATTEMPT_RESULT_RESTARTING = 5;
    private static final int METRICS_ATTEMPT_RESULT_FINISHING = 6;
    private static final int METRICS_ATTEMPT_RESULT_NO_WEB_CONTENTS = 7;
    // Obsolete: private static final int METRICS_ATTEMPT_RESULT_NO_VIDEO = 8;
    private static final int METRICS_ATTEMPT_RESULT_COUNT = 9;

    private static final String METRICS_END_REASON = "Media.VideoPersistence.EndReason";
    private static final int METRICS_END_REASON_RESUME = 0;
    // Obsolete: METRICS_END_REASON_NAVIGATION = 1;
    private static final int METRICS_END_REASON_CLOSE = 2;
    private static final int METRICS_END_REASON_CRASH = 3;
    private static final int METRICS_END_REASON_NEW_TAB = 4;
    private static final int METRICS_END_REASON_REPARENT = 5;
    private static final int METRICS_END_REASON_LEFT_FULLSCREEN = 6;
    private static final int METRICS_END_REASON_WEB_CONTENTS_LEFT_FULLSCREEN = 7;
    private static final int METRICS_END_REASON_COUNT = 8;

    private static final float MIN_ASPECT_RATIO = 1 / 2.39f;
    private static final float MAX_ASPECT_RATIO = 2.39f;

    /** Callbacks to cleanup after leaving PiP. */
    private final List<Runnable> mOnLeavePipCallbacks = new LinkedList<>();
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

    private static void recordAttemptResult(int result) {
        RecordHistogram.recordEnumeratedHistogram(
                METRICS_ATTEMPT_RESULT, result, METRICS_ATTEMPT_RESULT_COUNT);
    }

    private boolean shouldAttempt() {
        WebContents webContents = getWebContents();
        if (webContents == null) {
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_WEB_CONTENTS);
            return false;
        }

        // Non-null WebContents implies the native library has been loaded.
        assert LibraryLoader.getInstance().isInitialized();

        // Only auto-PiP if there is a playing fullscreen video that allows PiP.
        if (!webContents.hasActiveEffectivelyFullscreenVideo()
                || !webContents.isPictureInPictureAllowedForFullscreenVideo()) {
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_SYSTEM_SUPPORT);
            return false;
        }

        if (!mActivity.getPackageManager().hasSystemFeature(
                    PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            Log.d(TAG, "Activity does not have PiP feature.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_FEATURE);
            return false;
        }

        // Don't PiP if we are already in PiP.
        if (mActivity.isInPictureInPictureMode()) {
            Log.d(TAG, "Activity is already in PiP.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_ALREADY_RUNNING);
            return false;
        }

        // This means the activity is going to be restarted, so don't PiP.
        if (mActivity.isChangingConfigurations()) {
            Log.d(TAG, "Activity is being restarted.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_RESTARTING);
            return false;
        }

        // Don't PiP if the activity is finishing.
        if (mActivity.isFinishing()) {
            Log.d(TAG, "Activity is finishing.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_FINISHING);
            return false;
        }

        recordAttemptResult(METRICS_ATTEMPT_RESULT_SUCCESS);
        return true;
    }

    /**
     * Attempt to enter Picture in Picture mode if there is fullscreen video.
     */
    public void attemptPictureInPicture() {
        if (!shouldAttempt()) return;

        // Inform the WebContents when we enter and when we leave PiP.
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

        webContents.setHasPersistentVideo(true);

        // Setup observers to dismiss the Activity on events that should end PiP.
        final Tab activityTab = mActivityTabProvider.get();

        // We don't want InfoBars displaying while in PiP, they cover too much content.
        InfoBarContainer.get(activityTab).setHidden(true);

        mOnLeavePipCallbacks.add(() -> {
            webContents.setHasPersistentVideo(false);
            InfoBarContainer.get(activityTab).setHidden(false);
        });

        TabObserver tabObserver = new DismissActivityOnTabEventObserver(mActivity);
        ActivityTabProvider.ActivityTabObserver activityTabObserver =
                new DismissActivityOnTabChangeObserver(mActivity, activityTab);
        WebContentsObserver webContentsObserver =
                new DismissActivityOnWebContentsObserver(mActivity);
        FullscreenManager.Observer fullscreenListener = new FullscreenManager.Observer() {
            @Override
            public void onExitFullscreen(Tab tab) {
                dismissActivity(mActivity, METRICS_END_REASON_LEFT_FULLSCREEN);
            }
        };

        activityTab.addObserver(tabObserver);
        webContents.addObserver(webContentsObserver);
        mFullscreenManager.addObserver(fullscreenListener);
        mActivityTabProvider.addObserverAndTrigger(activityTabObserver);

        mOnLeavePipCallbacks.add(() -> {
            activityTab.removeObserver(tabObserver);
            webContents.removeObserver(webContentsObserver);
            mFullscreenManager.removeObserver(fullscreenListener);
            mActivityTabProvider.removeObserver(activityTabObserver);
        });

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
        cleanup(METRICS_END_REASON_RESUME);
    }

    private void cleanup(int reason) {
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

        RecordHistogram.recordEnumeratedHistogram(
                METRICS_END_REASON, reason, METRICS_END_REASON_COUNT);
    }

    /** Moves the Activity to the back and performs all cleanup. */
    private void dismissActivity(Activity activity, int reason) {
        activity.moveTaskToBack(true);
        cleanup(reason);
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
        public DismissActivityOnTabEventObserver(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
            if (window != null) dismissActivity(mActivity, METRICS_END_REASON_REPARENT);
        }

        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            dismissActivity(mActivity, METRICS_END_REASON_CLOSE);
        }

        @Override
        public void onCrash(Tab tab) {
            dismissActivity(mActivity, METRICS_END_REASON_CRASH);
        }

        @Override
        public void webContentsWillSwap(Tab tab) {
            dismissActivity(mActivity, METRICS_END_REASON_WEB_CONTENTS_LEFT_FULLSCREEN);
        }
    }

    /** A class to dismiss the Activity when the tab changes. */
    private class DismissActivityOnTabChangeObserver
            extends ActivityTabProvider.HintlessActivityTabObserver {
        private final Activity mActivity;
        private final Tab mCurrentTab;

        private DismissActivityOnTabChangeObserver(Activity activity, Tab currentTab) {
            mActivity = activity;
            mCurrentTab = currentTab;
        }

        @Override
        public void onActivityTabChanged(Tab tab) {
            if (mCurrentTab == tab) return;
            dismissActivity(mActivity, METRICS_END_REASON_NEW_TAB);
        }
    }

    /**
     * A class to dismiss the Activity when the Web Contents stops being effectively fullscreen.
     * This catches page navigations but unlike TabObserver's onNavigationFinished will not trigger
     * if an iframe without the fullscreen video navigates.
     */
    private class DismissActivityOnWebContentsObserver extends WebContentsObserver {
        private final Activity mActivity;

        public DismissActivityOnWebContentsObserver(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
            if (isFullscreen) return;

            dismissActivity(mActivity, METRICS_END_REASON_WEB_CONTENTS_LEFT_FULLSCREEN);
        }
    }
}
