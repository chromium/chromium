// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet;

import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS_ON_CLICK;
import static org.chromium.chrome.browser.educational_tip.two_cell.see_more_bottomsheet.EducationalTipSetupListBottomSheetProperties.BOTTOM_SHEET_TITLE;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;
import java.util.function.Supplier;

/**
 * Coordinator for the bottom sheet container in the two-cell educational tip. It is responsible for
 * fetching and preparing the content for the bottom sheet.
 */
@NullMarked
public class EducationalTipSetupListBottomSheetCoordinator {
    private final Context mContext;
    private final BottomSheetContent mBottomSheetContent;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;

    private final Supplier<List<EducationalTipSetupListBottomSheetItem>>
            mRankedEducationalTipProviderSupplier;

    /**
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipSetupListBottomSheetCoordinator(
            EducationTipModuleActionDelegate actionDelegate,
            Supplier<List<EducationalTipSetupListBottomSheetItem>>
                    rankedEducationalTipProviderSupplier) {
        mRankedEducationalTipProviderSupplier = rankedEducationalTipProviderSupplier;
        mContext = actionDelegate.getContext();
        mBottomSheetController = actionDelegate.getBottomSheetController();
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.educational_tip_setup_list_see_more_bottom_sheet_layout,
                                /* root= */ null);
        mBottomSheetContent = new EducationalTipSetupListBottomSheetContent(mContext, contentView);

        mModel =
                new PropertyModel.Builder(EducationalTipSetupListBottomSheetProperties.ALL_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, contentView, EducationalTipSetupListBottomSheetViewBinder::bind);

        mModel.set(
                BOTTOM_SHEET_LIST_ITEMS_ON_CLICK, () -> dismissBottomSheet(/* animate= */ false));
    }

    public void showBottomSheet() {
        mModel.set(BOTTOM_SHEET_TITLE, mContext.getString(R.string.get_the_most_out_of_chrome));
        mModel.set(
                BOTTOM_SHEET_DESCRIPTION,
                mContext.getString(R.string.educational_tip_see_more_bottom_sheet_description));
        mModel.set(BOTTOM_SHEET_LIST_ITEMS, mRankedEducationalTipProviderSupplier.get());

        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    public void dismissBottomSheet(boolean animate) {
        mBottomSheetController.hideContent(mBottomSheetContent, animate);
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }
}
