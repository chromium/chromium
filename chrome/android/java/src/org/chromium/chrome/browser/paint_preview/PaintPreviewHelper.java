// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.os.SystemClock;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceNativeInitObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.content_public.browser.WebContents;

/**
 * Handles initialization of the Paint Preview tab observers.
 */
public class PaintPreviewHelper {
    /**
     * Tracks whether there has been an attempt to display a paint preview before. We use this to
     * only attempt to display a paint preview on the first tab restoration that happens after
     * Chrome startup.
     */
    private static boolean sHasAttemptedToShowOnRestore;

    /**
     * Tracks the activity creation time in ms from {@link SystemClock#elapsedRealtime}.
     */
    private static long sActivityCreationTimeMs;

    /**
     * Initializes the logic required for the Paint Preview on startup feature. Mainly, observes a
     * {@link TabModelSelector} to monitor for initialization completion.
     * @param activity The ChromeActivity that corresponds to the tabModelSelector.
     * @param tabModelSelector The TabModelSelector to observe.
     */
    public static void initialize(ChromeActivity activity, TabModelSelector tabModelSelector) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) return;

        if (!MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity)) {
            sHasAttemptedToShowOnRestore = false;
        }
        sActivityCreationTimeMs = activity.getOnCreateTimestampMs();

        activity.getLifecycleDispatcher().register(
                new PaintPreviewTabServiceNativeInitObserver(activity.getLifecycleDispatcher()));

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
     * Attempts to display the Paint Preview representation of for the given Tab.
     * @param onShown The callback for when the Paint Preview is shown.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     * @return Whether the Paint Preview started to initialize or is already initializating.
     * Note that if the Paint Preview is already showing, this will return false.
     */
    public static boolean showPaintPreviewOnRestore(
            Tab tab, Runnable onShown, Runnable onDismissed) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)
                || sHasAttemptedToShowOnRestore
                || ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            return false;
        }

        sHasAttemptedToShowOnRestore = true;

        TabbedPaintPreviewPlayer player = TabbedPaintPreviewPlayer.get(tab);
        PageLoadMetrics.Observer observer = new PageLoadMetrics.Observer() {
            @Override
            public void onFirstMeaningfulPaint(WebContents webContents, long navigationId,
                    long navigationStartTick, long firstMeaningfulPaintMs) {
                player.onFirstMeaningfulPaint(webContents);
            }
        };

        if (!player.maybeShow(onShown, () -> {
                onDismissed.run();
                PageLoadMetrics.removeObserver(observer);
            }, sActivityCreationTimeMs, UmaUtils::hasComeToBackground)) {
            return false;
        }

        PageLoadMetrics.addObserver(observer);

        return true;
    }
}
