// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the bottom sheet container in the two-cell educational tip. It is responsible for
 * fetching and preparing the content for the bottom sheet.
 */
@NullMarked
public class EducationalTipBottomSheetCoordinator {
    private final BottomSheetContent mBottomSheetContent;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;

    /**
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipBottomSheetCoordinator(EducationTipModuleActionDelegate actionDelegate) {
        mBottomSheetController = actionDelegate.getBottomSheetController();
        View contentView =
                LayoutInflater.from(actionDelegate.getContext())
                        .inflate(
                                R.layout.educational_tip_setup_list_see_more_bottom_sheet_layout,
                                /* root= */ null);
        mBottomSheetContent = new EducationalTipBottomSheetContent(contentView);

        mModel = new PropertyModel.Builder(EducationalTipBottomSheetProperties.ALL_KEYS).build();
    }

    public void showBottomSheet() {
        // TODO(crbug.com/479597724): Set title and description based on number of completed items.
        // Determines order of module types.
        mModel.set(
                EducationalTipBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS,
                SetupListModuleUtils.getRankedModuleTypes());

        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }
}
