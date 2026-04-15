// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Controller for page summary toolbar button. */
@NullMarked
public class PageSummaryButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final AiAssistantService mAiAssistantService;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;

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
            Supplier<@Nullable Tab> activeTabSupplier,
            AiAssistantService aiAssistantService,
            Supplier<@Nullable Tracker> tracker) {
        super(
                activeTabSupplier,
                modalDialogManager,
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(context, R.drawable.summarize_auto),
                                context.getString(R.string.menu_summarize_with_ai),
                                /* supportsTinting= */ true)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.PAGE_SUMMARY)
                        .setHoverTooltipTextId(R.string.menu_summarize_with_ai)
                        .build());
        mContext = context;
        mAiAssistantService = aiAssistantService;
        mTrackerSupplier = tracker;

        mPageSummarySpec = mButtonData.getButtonSpec();
        mReviewPdfSpec =
                new ButtonSpec.Builder(
                                mPageSummarySpec.getDrawable(),
                                context.getString(R.string.menu_review_pdf_with_ai),
                                /* supportsTinting= */ true)
                        .setOnClickListener(this)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.PAGE_SUMMARY)
                        .setHoverTooltipTextId(R.string.menu_review_pdf_with_ai)
                        .build();
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        var isPdfPage = isPdfPage(tab);
        mButtonData.setButtonSpec(isPdfPage ? mReviewPdfSpec : mPageSummarySpec);
        return super.get(tab);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        return super.shouldShowButton(tab) && mAiAssistantService.canShowAiForTab(mContext, tab);
    }

    @Override
    public void onClick(View view) {
        var activeTab = mActiveTabSupplier.get();
        assert activeTab != null;
        var trackerEvent =
                isPdfPage(activeTab)
                        ? EventConstants.ADAPTIVE_TOOLBAR_PAGE_SUMMARY_PDF_USED
                        : EventConstants.ADAPTIVE_TOOLBAR_PAGE_SUMMARY_WEB_USED;
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(trackerEvent);
        }

        mAiAssistantService.showAi(mContext, activeTab);
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        var tabIsPdf = isPdfPage(tab);
        var featureName =
                tabIsPdf
                        ? FeatureConstants
                                .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_PAGE_SUMMARY_PDF_FEATURE
                        : FeatureConstants
                                .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_PAGE_SUMMARY_WEB_FEATURE;
        var stringId =
                tabIsPdf
                        ? R.string.adaptive_toolbar_button_review_pdf_iph
                        : R.string.adaptive_toolbar_button_page_summary_iph;

        return new IphCommandBuilder(
                tab.getContext().getResources(),
                featureName,
                /* stringId= */ stringId,
                /* accessibilityStringId= */ stringId);
    }

    private boolean isPdfPage(@Nullable Tab tab) {
        return tab != null && tab.getNativePage() instanceof PdfPage;
    }
}
