// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Responsible for providing UI resources for showing price insights action on optional toolbar
 * button.
 */
public class PriceInsightsButtonController extends BaseButtonDataProvider {

    private final SnackbarManager mSnackbarManager;
    private final BottomSheetController mBottomSheetController;
    private final ButtonSpec mButtonSpec;

    public PriceInsightsButtonController(
            Context context,
            Supplier<Tab> tabSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            Drawable buttonDrawable) {
        super(
                tabSupplier,
                modalDialogManager,
                buttonDrawable,
                /* contentDescriptionResId= */ context.getString(R.string.price_insights_title),
                /* actionChipLabelResId= */ R.string.price_insights_price_is_low_title,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);

        mButtonSpec = mButtonData.getButtonSpec();
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void onClick(View view) {
        // TODO(b/336825059): Present price insights bottom sheet controller.
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.price_insights_price_is_low_title,
                        /* accessibilityStringId= */ R.string.price_insights_price_is_low_title);
        return iphCommandBuilder;
    }
}
