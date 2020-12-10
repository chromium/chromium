// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.app.Activity;
import android.content.Context;
import android.os.SystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
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
 * Glue code for the Paint Preview show-on-startup feature.
 */
public class StartupPaintPreviewHelper {
    /**
     * Tracks whether a paint preview should be shown on tab restore. We use this to only attempt
     * to display a paint preview on the first tab restoration that happens on Chrome startup when
     * cold.
     */
    private static boolean sShouldShowOnRestore;

    /**
     * A map for keeping Activity-specific variables and classes. New entries are added on calls to
     * {@link #initialize}. Entries are automatically removed when their respective Activity is
     * destroyed.
     */
    private static final Map<WindowAndroid, PaintPreviewWindowAndroidHelper>
            sWindowAndroidHelperMap = new HashMap<>();

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
     * @param windowAndroid The WindowAndroid that corresponds to the tabModelSelector.
     * @param activityCreationTime The time the ChromeActivity was created.
     * @param browserControlsManager The BrowserControlsManager which is used to fetch the browser
     *         visibility delegate
     * @param tabModelSelector The TabModelSelector to observe.
     * @param willShowStartSurface Whether the start surface will be shown.
     * @param progressBarCoordinatorSupplier Supplier for the progress bar.
     */
    public static void initialize(WindowAndroid windowAndroid, long activityCreationTime,
            BrowserControlsManager browserControlsManager, TabModelSelector tabModelSelector,
            boolean willShowStartSurface,
            Supplier<LoadProgressCoordinator> progressBarCoordinatorSupplier,
            Callback<Long> visibleContentCallback) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) return;

        if (MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(
                    windowAndroid.getContext().get())
                || willShowStartSurface) {
            sShouldShowOnRestore = false;
        }
        sWindowAndroidHelperMap.put(windowAndroid,
                new PaintPreviewWindowAndroidHelper(windowAndroid, activityCreationTime,
                        browserControlsManager, progressBarCoordinatorSupplier,
                        visibleContentCallback));

        // TODO(crbug/1074428): verify this doesn't cause a memory leak if the user exits Chrome
        // prior to onTabStateInitialized being called.
        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                // If the first tab shown is not a normal tab, then prevent showing previews in the
                // future.
                if (preventShowOnRestore(tabModelSelector.getCurrentTab())) {
                    sShouldShowOnRestore = false;
                }

                Context context = windowAndroid.getContext().get();
                boolean runAudit = context == null
                        || !MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(
                                context);
                // Avoid running the audit in multi-window mode as otherwise we will delete
                // data that is possibly in use by the other Activity's TabModelSelector.
                PaintPreviewTabServiceFactory.getServiceInstance().onRestoreCompleted(
                        tabModelSelector, runAudit, /*captureOnSwitch=*/false);
                tabModelSelector.removeObserver(this);
            }

            private boolean preventShowOnRestore(Tab tab) {
                if (tab == null || tab.isShowingErrorPage() || tab.isNativePage()) {
                    return true;
                }

                String scheme = tab.getUrl().getScheme();
                boolean httpOrHttps = scheme.equals("http") || scheme.equals("https");
                return !httpOrHttps;
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
                progressSimulatorCallback, progressPreventionCallback,
                windowAndroidHelper.getVisibleContentCallback());
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
        private final long mActivityCreationTime;
        private final WindowAndroid mWindowAndroid;
        private final BrowserControlsManager mBrowserControlsManager;
        private final Supplier<LoadProgressCoordinator> mProgressBarCoordinatorSupplier;
        private final Callback<Long> mVisibleContentCallback;

        PaintPreviewWindowAndroidHelper(WindowAndroid windowAndroid, long activityCreationTime,
                BrowserControlsManager browserControlsManager,
                Supplier<LoadProgressCoordinator> progressBarCoordinatorSupplier,
                Callback<Long> visibleContentCallback) {
            mWindowAndroid = windowAndroid;
            mActivityCreationTime = activityCreationTime;
            mBrowserControlsManager = browserControlsManager;
            mProgressBarCoordinatorSupplier = progressBarCoordinatorSupplier;
            mVisibleContentCallback = visibleContentCallback;
            ApplicationStatus.registerStateListenerForActivity(
                    this, mWindowAndroid.getActivity().get());
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

        Callback<Long> getVisibleContentCallback() {
            return mVisibleContentCallback;
        }

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.DESTROYED) {
                sWindowAndroidHelperMap.remove(mWindowAndroid);
                ApplicationStatus.unregisterActivityStateListener(this);
            }
        }
    }

    private StartupPaintPreviewHelper() {}
}
