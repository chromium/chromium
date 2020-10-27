// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.app.Activity;
import android.os.SystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressCoordinator;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Handles initialization of the Paint Preview tab observers.
 */
public class PaintPreviewHelper {
    /**
     * Tracks whether a paint preview should be shown on tab restore. We use this to only attempt
     * to display a paint preview on the first tab restoration that happens on Chrome startup when
     * cold.
     */
    private static boolean sShouldShowOnRestore;

    /**
     * A map for keeping Activity-specific variables and classes. New entries are added on calls to
     * {@link #initialize(ChromeActivity, TabModelSelector)}. Entries are automatically removed when
     * their respective Activity is destroyed.
     */
    private static Map<WindowAndroid, PaintPreviewWindowAndroidHelper> sWindowAndroidHelperMap =
            new HashMap<>();

    /**
     * Sets whether a Paint Preview should attempt to be shown on restoration of a tab. If the
     * feature is not enabled this is effectively a no-op.
     */
    public static void setShouldShowOnRestore(boolean shouldShowOnRestore) {
        sShouldShowOnRestore = shouldShowOnRestore;
    }

    /**
     * Initializes the logic required for the Paint Preview on startup feature. Mainly, observes a
     * {@link TabModelSelector} to monitor for initialization completion.
     *
     * @param activity         The ChromeActivity that corresponds to the tabModelSelector.
     * @param tabModelSelector The TabModelSelector to observe.
     * @param willShowStartSurface Whether the start surface will be shown.
     * @param progressBarCoordinatorSupplier Supplier for the progress bar.
     */
    public static void initialize(ChromeActivity<?> activity, TabModelSelector tabModelSelector,
            boolean willShowStartSurface,
            Supplier<LoadProgressCoordinator> progressBarCoordinatorSupplier) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) return;

        if (MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity)
                || willShowStartSurface) {
            sShouldShowOnRestore = false;
        }
        sWindowAndroidHelperMap.put(activity.getWindowAndroid(),
                new PaintPreviewWindowAndroidHelper(activity, progressBarCoordinatorSupplier));

        // TODO(crbug/1074428): verify this doesn't cause a memory leak if the user exits Chrome
        // prior to onTabStateInitialized being called.
        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                // Avoid running the audit in multi-window mode as otherwise we will delete
                // data that is possibly in use by the other Activity's TabModelSelector.
                PaintPreviewTabServiceFactory.getServiceInstance().onRestoreCompleted(
                        tabModelSelector, /*runAudit=*/
                        !MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity),
                        /*captureOnSwitch=*/false);
                tabModelSelector.removeObserver(this);
            }
        });
    }

    /**
     * Attempts to display the Paint Preview representation for the given Tab.
     */
    public static void showPaintPreviewOnRestore(Tab tab) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)
                || !sShouldShowOnRestore
                || ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            return;
        }

        PaintPreviewWindowAndroidHelper windowAndroidHelper =
                sWindowAndroidHelperMap.get(tab.getWindowAndroid());
        if (windowAndroidHelper == null) return;

        sShouldShowOnRestore = false;
        Runnable progressSimulatorCallback = () -> {
            if (windowAndroidHelper.getLoadProgressCoordinator() == null) return;
            windowAndroidHelper.getLoadProgressCoordinator().simulateLoadProgressCompletion();
        };
        Callback<Boolean> progressPreventionCallback = (preventProgressbar) -> {
            if (windowAndroidHelper.getLoadProgressCoordinator() == null) return;
            windowAndroidHelper.getLoadProgressCoordinator().setPreventUpdates(preventProgressbar);
        };

        StartupPaintPreview startupPaintPreview = new StartupPaintPreview(tab,
                windowAndroidHelper.getBrowserControlsManager().getBrowserVisibilityDelegate(),
                progressSimulatorCallback, progressPreventionCallback);
        startupPaintPreview.setActivityCreationTimestampMs(
                windowAndroidHelper.getActivityCreationTime());
        startupPaintPreview.setShouldRecordFirstPaint(
                () -> UmaUtils.hasComeToForeground() && !UmaUtils.hasComeToBackground());
        startupPaintPreview.setIsOfflinePage(() -> OfflinePageUtils.isOfflinePage(tab));
        PageLoadMetrics.Observer observer = new PageLoadMetrics.Observer() {
            @Override
            public void onFirstMeaningfulPaint(WebContents webContents, long navigationId,
                    long navigationStartTick, long firstMeaningfulPaintMs) {
                startupPaintPreview.onWebContentsFirstMeaningfulPaint(webContents);
            }
        };
        PageLoadMetrics.addObserver(observer);
        startupPaintPreview.show(() -> PageLoadMetrics.removeObserver(observer));
    }

    /**
     * A helper class for keeping activity-specific variables and classes.
     */
    private static class PaintPreviewWindowAndroidHelper
            implements ApplicationStatus.ActivityStateListener {
        /**
         * Tracks the activity creation time in ms from {@link SystemClock#elapsedRealtime}.
         */
        private long mActivityCreationTime;
        private WindowAndroid mWindowAndroid;
        private BrowserControlsManager mBrowserControlsManager;
        private Supplier<LoadProgressCoordinator> mProgressBarCoordinatorSupplier;

        PaintPreviewWindowAndroidHelper(ChromeActivity<?> chromeActivity,
                Supplier<LoadProgressCoordinator> progressBarCoordinatorSupplier) {
            mWindowAndroid = chromeActivity.getWindowAndroid();
            mActivityCreationTime = chromeActivity.getOnCreateTimestampMs();
            mBrowserControlsManager = chromeActivity.getBrowserControlsManager();
            mProgressBarCoordinatorSupplier = progressBarCoordinatorSupplier;
            ApplicationStatus.registerStateListenerForActivity(this, chromeActivity);
        }

        long getActivityCreationTime() {
            return mActivityCreationTime;
        }

        LoadProgressCoordinator getLoadProgressCoordinator() {
            return mProgressBarCoordinatorSupplier.get();
        }

        BrowserControlsManager getBrowserControlsManager() {
            return mBrowserControlsManager;
        }

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.DESTROYED) {
                sWindowAndroidHelperMap.remove(mWindowAndroid);
                ApplicationStatus.unregisterActivityStateListener(this);
            }
        }
    }

    private PaintPreviewHelper() {}
}
