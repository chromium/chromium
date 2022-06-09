// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.segmentation_platform.OnDemandSegmentSelectionResult;
import org.chromium.components.segmentation_platform.PageLoadTriggerContext;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.content_public.browser.WebContents;

/**
 * Central class for contextual page actions bridging between UI and backend. Registers itself with
 * segmentation platform for on-demand model execution on page load triggers. Provides updated
 * button data to the toolbar when asked for it.
 */
public class ContextualPageActionController {
    private static final String CONTEXTUAL_PAGE_ACTION_SEGMENTATION_KEY = "contextual_page_action";

    private SegmentationPlatformService mSegmentationPlatformService;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private int mSegmentSelectionCallbackId;

    /**
     * Constructor.
     * @param profileSupplier The supplier for current profile.
     * @param tabSupplier The supplier of the current tab.
     * @param adaptiveToolbarButtonController The {@link AdaptiveToolbarButtonController} that
     *         handles the logic to decide between multiple buttons to show.
     */
    public ContextualPageActionController(ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> tabSupplier,
            AdaptiveToolbarButtonController adaptiveToolbarButtonController) {
        mTabSupplier = tabSupplier;
        mAdaptiveToolbarButtonController = adaptiveToolbarButtonController;
        profileSupplier.addObserver(profile -> {
            if (profile.isOffTheRecord()) return;
            if (!AdaptiveToolbarFeatures.isContextualPageActionUiEnabled()) return;

            mSegmentationPlatformService =
                    SegmentationPlatformServiceFactory.getForProfile(profile);
            mSegmentSelectionCallbackId =
                    mSegmentationPlatformService.registerOnDemandSegmentSelectionCallback(
                            CONTEXTUAL_PAGE_ACTION_SEGMENTATION_KEY,
                            this::onSegmentSelectionResult);
        });
    }

    /** Called on destroy. */
    public void destroy() {
        if (mSegmentationPlatformService == null) return;
        mSegmentationPlatformService.unregisterOnDemandSegmentSelectionCallback(
                CONTEXTUAL_PAGE_ACTION_SEGMENTATION_KEY, mSegmentSelectionCallbackId);
    }

    private void onSegmentSelectionResult(OnDemandSegmentSelectionResult result) {
        if (result == null || !(result.triggerContext instanceof PageLoadTriggerContext)) return;
        WebContents webContents = ((PageLoadTriggerContext) result.triggerContext).webContents;
        Tab processedTab = webContents == null || webContents.isDestroyed()
                ? null
                : TabUtils.fromWebContents(webContents);
        Tab currentTab = mTabSupplier.get();
        boolean isSameTab = currentTab != null && processedTab != null
                && currentTab.getId() == processedTab.getId();
        if (!isSameTab) return;
        mAdaptiveToolbarButtonController.showDynamicAction(
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        result.segmentSelectionResult.selectedSegment));
    }
}
