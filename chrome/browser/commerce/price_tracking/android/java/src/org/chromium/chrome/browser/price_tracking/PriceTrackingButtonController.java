// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
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
                /*supportsTinting=*/true, /*iphCommandBuilder*/ null,
                AdaptiveToolbarButtonVariant.PRICE_TRACKING);
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        maybeSetActionChipResourceId();
        return super.get(tab);
    }

    private void maybeSetActionChipResourceId() {
        if (FeatureList.isInitialized() && AdaptiveToolbarFeatures.shouldShowActionChip()) {
            // OptionalButtonCoordinator may choose to not show this action chip. It uses feature
            // engagement to rate limit this animation.
            mButtonData.updateActionChipResourceId(R.string.enable_price_tracking_menu_item);
        } else {
            mButtonData.updateActionChipResourceId(Resources.ID_NULL);
        }
    }

    @Override
    public void onClick(View view) {
        mTabBookmarkerSupplier.get().startOrModifyPriceTracking(mActiveTabSupplier.get());
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        if (AdaptiveToolbarFeatures.shouldShowActionChip()) {
            return null;
        }

        IPHCommandBuilder iphCommandBuilder = new IPHCommandBuilder(tab.getContext().getResources(),
                FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING,
                /* stringId = */ R.string.iph_price_tracking_menu_item,
                /* accessibilityStringId = */ R.string.iph_price_tracking_menu_item);
        return iphCommandBuilder;
    }
}
