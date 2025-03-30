// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Controller for page summary toolbar button. */
public class PageSummaryButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final AiAssistantService mAiAssistantService;

    private final ButtonSpec mPageSummarySpec;
    private final ButtonSpec mReviewPdfSpec;

    /**
     * Creates an instance of PageSummaryButtonController.
     *
     * @param context Android context, used to get resources.
     * @param activeTabSupplier Active tab supplier.
     * @param aiAssistantService Summarization controller, handles summarization flow.
     */
    public PageSummaryButtonController(
            Context context,
            ModalDialogManager modalDialogManager,
            Supplier<Tab> activeTabSupplier,
            AiAssistantService aiAssistantService) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.summarize_auto),
                /* contentDescription= */ context.getString(R.string.menu_summarize_with_ai),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                /* tooltipTextResId= */ R.string.menu_summarize_with_ai,
                /* showHoverHighlight= */ true);
        mContext = context;
        mAiAssistantService = aiAssistantService;

        mPageSummarySpec = mButtonData.getButtonSpec();
        mReviewPdfSpec =
                new ButtonSpec(
                        mPageSummarySpec.getDrawable(),
                        /* onClickListener= */ this,
                        /* onLongClickListener= */ null,
                        /* contentDescription= */ context.getString(
                                R.string.menu_review_pdf_with_ai),
                        /* supportsTinting= */ true,
                        /* iphCommandBuilder= */ null,
                        AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ R.string.menu_review_pdf_with_ai,
                        /* showHoverHighlight= */ true,
                        /* hasErrorBadge= */ false);
    }

    @Override
    public ButtonData get(Tab tab) {
        var isPdfPage = tab != null && tab.getNativePage() instanceof PdfPage;
        mButtonData.setButtonSpec(isPdfPage ? mReviewPdfSpec : mPageSummarySpec);
        return super.get(tab);
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        return super.shouldShowButton(tab) && mAiAssistantService.canShowAiForTab(mContext, tab);
    }

    @Override
    public void onClick(View view) {
        assert mActiveTabSupplier.hasValue() : "Active tab supplier should have a value";

        mAiAssistantService.showAi(mContext, mActiveTabSupplier.get());
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return super.getIphCommandBuilder(tab);
    }
}
