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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Controller for page summary toolbar button. */
public class PageSummaryButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final AiAssistantService mAiAssistantService;

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
                context.getString(R.string.sharing_create_summary),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PAGE_SUMMARY,
                /* tooltipTextResId= */ R.string.sharing_create_summary,
                /* showHoverHighlight= */ true);
        mContext = context;
        mAiAssistantService = aiAssistantService;
    }

    @Override
    public void onClick(View view) {
        assert mActiveTabSupplier.hasValue() : "Active tab supplier should have a value";

        mAiAssistantService.showAi(mContext, mActiveTabSupplier.get());
    }
}
