// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Controller for page summary toolbar button. */
public class PageSummaryButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PageInfoSharingController mPageInfoSharingController;

    /**
     * Creates an instance of PageSummaryButtonController.
     *
     * @param context Android context, used to get resources.
     * @param bottomSheetController Bottom sheet controller, used to show summarization UI.
     * @param activeTabSupplier Active tab supplier.
     * @param pageInfoSharingController Summarization controller, handles summarization flow.
     */
    public PageSummaryButtonController(
            Context context,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            Supplier<Tab> activeTabSupplier,
            PageInfoSharingController pageInfoSharingController) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_mobile_friendly),
                context.getString(R.string.sharing_create_summary),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                /* tooltipTextResId= */ R.string.sharing_create_summary,
                /* showHoverHighlight= */ true);
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mPageInfoSharingController = pageInfoSharingController;
    }

    @Override
    public void onClick(View view) {
        assert mActiveTabSupplier.hasValue() : "Active tab supplier should have a value";

        mPageInfoSharingController.sharePageInfo(
                mContext,
                mBottomSheetController,
                /* chromeOptionShareCallback= */ null, // TODO(b/364948892): Decouple sharing
                // functionality.
                mActiveTabSupplier.get());
    }
}
