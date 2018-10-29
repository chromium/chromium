// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.util.Rational;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.lang.reflect.InvocationTargetException;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A controller for entering Android O Picture in Picture mode with fullscreen videos.
 */
@SuppressLint({"NewApi"})
public class PictureInPictureController {
    private static final String TAG = "VideoPersist";

    // Metrics
    private static final String METRICS_URL = "Media.VideoPersistence.TopFrame";
    private static final String METRICS_DURATION = "Media.VideoPersistence.Duration";

    private static final String METRICS_ATTEMPT_RESULT = "Media.VideoPersistence.AttemptResult";
    private static final int METRICS_ATTEMPT_RESULT_SUCCESS = 0;
    private static final int METRICS_ATTEMPT_RESULT_NO_SYSTEM_SUPPORT = 1;
    private static final int METRICS_ATTEMPT_RESULT_NO_FEATURE = 2;
    private static final int METRICS_ATTEMPT_RESULT_NO_ACTIVITY_SUPPORT = 3;
    private static final int METRICS_ATTEMPT_RESULT_ALREADY_RUNNING = 4;
    private static final int METRICS_ATTEMPT_RESULT_RESTARTING = 5;
    private static final int METRICS_ATTEMPT_RESULT_FINISHING = 6;
    private static final int METRICS_ATTEMPT_RESULT_NO_WEBCONTENTS = 7;
    private static final int METRICS_ATTEMPT_RESULT_NO_VIDEO = 8;
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
    private List<Callback<ChromeActivity>> mOnLeavePipCallbacks = new LinkedList<>();

    /**
     * Convenience method to get the {@link WebContents} from the active Tab of a
     * {@link ChromeActivity}.
     */
    @Nullable
    private static WebContents getWebContents(ChromeActivity activity) {
        if (!activity.areTabModelsInitialized()) return null;
        assert activity.getTabModelSelector() != null;
        if (activity.getActivityTab() == null) return null;
        return activity.getActivityTab().getWebContents();
    }

    private static void recordAttemptResult(int result) {
        RecordHistogram.recordEnumeratedHistogram(
                METRICS_ATTEMPT_RESULT, result, METRICS_ATTEMPT_RESULT_COUNT);
    }

    private boolean shouldAttempt(ChromeActivity activity) {
        WebContents webContents = getWebContents(activity);
        if (webContents == null) return false;

        // Non-null WebContents implies the native library has been loaded.
        assert LibraryLoader.getInstance().isInitialized();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.VIDEO_PERSISTENCE)) return false;

        // Only auto-PiP if there is a playing fullscreen video that allows PiP.
        if (!webContents.hasActiveEffectivelyFullscreenVideo()
                || !webContents.isPictureInPictureAllowedForFullscreenVideo()) {
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_VIDEO);
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_SYSTEM_SUPPORT);
            return false;
        }

        if (!activity.getPackageManager().hasSystemFeature(
                    PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            Log.d(TAG, "Activity does not have PiP feature.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_FEATURE);
            return false;
        }

        // TODO(peconn): Clean this up when we build with the O SDK.
        try {
            ActivityInfo info =
                    activity.getPackageManager().getActivityInfo(activity.getComponentName(), 0);
            Boolean supports =
                    (Boolean) info.getClass().getMethod("supportsPictureInPicture").invoke(info);
            if (!supports) {
                Log.d(TAG, "Activity does not support PiP.");
                recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_ACTIVITY_SUPPORT);
                return false;
            }
        } catch (PackageManager.NameNotFoundException | IllegalAccessException
                | NoSuchMethodException | InvocationTargetException e) {
            Log.d(TAG, "Exception checking whether Activity supports PiP.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_NO_ACTIVITY_SUPPORT);
            return false;
        }

        // Don't PiP if we are already in PiP.
        if (activity.isInPictureInPictureMode()) {
            Log.d(TAG, "Activity is already in PiP.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_ALREADY_RUNNING);
            return false;
        }

        // This means the activity is going to be restarted, so don't PiP.
        if (activity.isChangingConfigurations()) {
            Log.d(TAG, "Activity is being restarted.");
            recordAttemptResult(METRICS_ATTEMPT_RESULT_RESTARTING);
            return false;
        }

        // Don't PiP if the activity is finishing.
        if (activity.isFinishing()) {
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
    public void attemptPictureInPicture(final ChromeActivity activity) {
        if (!shouldAttempt(activity)) return;

        // Inform the WebContents when we enter and when we leave PiP.
        final WebContents webContents = getWebContents(activity);
        assert webContents != null;

        Rect bounds = getVideoBounds(webContents, activity);
        PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
        if (bounds != null) {
            builder.setAspectRatio(new Rational(bounds.width(), bounds.height()));
            builder.setSourceRectHint(bounds);
        }

        try {
            if (!activity.enterPictureInPictureMode(builder.build())) return;
        } catch (IllegalStateException | IllegalArgumentException e) {
            Log.e(TAG, "Error entering PiP with bounds (%d, %d): %s",
                    bounds.width(), bounds.height(), e);
            return;
        }

        webContents.setHasPersistentVideo(true);

        // We don't want InfoBars displaying while in PiP, they cover too much content.
        InfoBarContainer.get(activity.getActivityTab()).setHidden(true);

        mOnLeavePipCallbacks.add(new Callback<ChromeActivity>() {
            @Override
            public void onResult(ChromeActivity activity2) {
                webContents.setHasPersistentVideo(false);
                InfoBarContainer.get(activity.getActivityTab()).setHidden(false);
            }
        });

        // Setup observers to dismiss the Activity on events that should end PiP.
        final Tab activityTab = activity.getActivityTab();

        RapporServiceBridge.sampleDomainAndRegistryFromURL(METRICS_URL, activityTab.getUrl());

        final TabObserver tabObserver = new DismissActivityOnTabEventObserver(activity);
        final TabModelSelectorObserver tabModelSelectorObserver =
                new DismissActivityOnTabModelSelectorEventObserver(activity);
        final WebContentsObserver webContentsObserver =
                new DismissActivityOnWebContentsObserver(activity);

        activityTab.addObserver(tabObserver);
        activityTab.getTabModelSelector().addObserver(tabModelSelectorObserver);
        webContents.addObserver(webContentsObserver);

        mOnLeavePipCallbacks.add(new Callback<ChromeActivity>() {
            @Override
            public void onResult(ChromeActivity activity2) {
                activityTab.removeObserver(tabObserver);
                activityTab.getTabModelSelector().removeObserver(tabModelSelectorObserver);
                webContents.removeObserver(webContentsObserver);
            }
        });

        final long startTimeMs = SystemClock.elapsedRealtime();
        mOnLeavePipCallbacks.add(new Callback<ChromeActivity>() {
            @Override
            public void onResult(ChromeActivity activity2) {
                long pipTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
                RecordHistogram.recordCustomTimesHistogram(METRICS_DURATION, pipTimeMs,
                        TimeUnit.SECONDS.toMillis(7), TimeUnit.HOURS.toMillis(10),
                        TimeUnit.MILLISECONDS, 50);
            }
        });
    }

    private static Rect getVideoBounds(WebContents webContents, Activity activity) {
        // |getFullscreenVideoSize| may return null if there is a fullscreen video but it has not
        // yet been detected. However we check |hasActiveEffectivelyFullscreenVideo| in
        // |shouldAttempt|, so |rect| should never be null.
        Rect rect = webContents.getFullscreenVideoSize();
        if (rect.width() == 0 || rect.height() == 0) return null;

        float videoAspectRatio = MathUtils.clamp(rect.width() / (float) rect.height(),
                MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);

        int windowWidth = activity.getWindow().getDecorView().getWidth();
        int windowHeight = activity.getWindow().getDecorView().getHeight();
        float phoneAspectRatio = windowWidth / (float) windowHeight;

        // The currently playing video size is the video frame size, not the on-screen size.
        // We know the video will be touching either the sides or the top and bottom of the screen
        // so we can work out the screen bounds of the video from this.
        int width, height;
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
    public void cleanup(ChromeActivity activity) {
        cleanup(activity, METRICS_END_REASON_RESUME);
    }

    private void cleanup(ChromeActivity activity, int reason) {
        // If `mOnLeavePipCallbacks` is empty, it means that the cleanup call happened while Chrome
        // was not PIP'ing. The early return also avoid recording the reason why the PIP session
        // ended.
        if (mOnLeavePipCallbacks.isEmpty()) return;

        // This method can be called when we haven't been PiPed. We use Callbacks to ensure we only
        // do cleanup if it is required.
        for (Callback<ChromeActivity> callback : mOnLeavePipCallbacks) {
            callback.onResult(activity);
        }
        mOnLeavePipCallbacks.clear();

        RecordHistogram.recordEnumeratedHistogram(
                METRICS_END_REASON, reason, METRICS_END_REASON_COUNT);
    }

    /** Moves the Activity to the back and performs all cleanup. */
    private void dismissActivity(ChromeActivity activity, int reason) {
        activity.moveTaskToBack(true);
        cleanup(activity, reason);
    }

    /**
     * A class to dismiss the Activity when the tab:
     * - Closes.
     * - Reparents: Attaches to a different activity.
     * - Crashes.
     * - Leaves fullscreen.
     */
    private class DismissActivityOnTabEventObserver extends EmptyTabObserver {
        private final ChromeActivity mActivity;
        public DismissActivityOnTabEventObserver(ChromeActivity activity) {
            mActivity = activity;
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
            if (isAttached) {
                dismissActivity(mActivity, METRICS_END_REASON_REPARENT);
            }
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
        public void onExitFullscreenMode(Tab tab) {
            dismissActivity(mActivity, METRICS_END_REASON_LEFT_FULLSCREEN);
        }
    }

    /** A class to dismiss the Activity when a new tab is created. */
    private class DismissActivityOnTabModelSelectorEventObserver
            extends EmptyTabModelSelectorObserver {
        private final ChromeActivity mActivity;
        private final Tab mTab;
        public DismissActivityOnTabModelSelectorEventObserver(ChromeActivity activity) {
            mActivity = activity;
            mTab = activity.getActivityTab();
        }

        @Override
        public void onChange() {
            if (mActivity.getActivityTab() != mTab) {
                dismissActivity(mActivity, METRICS_END_REASON_NEW_TAB);
            }
        }
    }

    /**
     * A class to dismiss the Activity when the Web Contents stops being effectively fullscreen.
     * This catches page navigations but unlike TabObserver's onNavigationFinished will not trigger
     * if an iframe without the fullscreen video navigates.
     */
    private class DismissActivityOnWebContentsObserver extends WebContentsObserver {
        private final ChromeActivity mActivity;

        public DismissActivityOnWebContentsObserver(ChromeActivity activity) {
            mActivity = activity;
        }

        @Override
        public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
            if (isFullscreen) return;

            dismissActivity(mActivity, METRICS_END_REASON_WEB_CONTENTS_LEFT_FULLSCREEN);
        }
    }
}
