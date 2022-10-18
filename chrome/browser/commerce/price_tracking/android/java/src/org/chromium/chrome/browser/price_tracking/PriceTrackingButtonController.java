// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Responsible for providing UI resources for showing price tracking action on optional toolbar
 * button.
 */
public class PriceTrackingButtonController extends BaseButtonDataProvider {
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;

    /** Constructor. */
    public PriceTrackingButtonController(ObservableSupplier<Tab> tabSupplier,
            ModalDialogManager modalDialogManager, Drawable buttonDrawable,
            Supplier<TabBookmarker> tabBookmarkerSupplier) {
        super(tabSupplier, modalDialogManager, buttonDrawable,
                R.string.enable_price_tracking_menu_item,
                /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                /*supportsTinting=*/true, /*iphCommandBuilder*/ null,
                AdaptiveToolbarButtonVariant.PRICE_TRACKING);
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
    }

    @Override
    public void onClick(View view) {
        mTabBookmarkerSupplier.get().startOrModifyPriceTracking(mActiveTabSupplier.get());
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        IPHCommandBuilder iphCommandBuilder = new IPHCommandBuilder(tab.getContext().getResources(),
                FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                /* stringId = */ R.string.iph_price_tracking_menu_item,
                /* accessibilityStringId = */ R.string.iph_price_tracking_menu_item);
        return iphCommandBuilder;
    }
}
