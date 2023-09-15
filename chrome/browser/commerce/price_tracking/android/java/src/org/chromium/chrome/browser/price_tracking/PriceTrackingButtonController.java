// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Responsible for providing UI resources for showing price tracking action on optional toolbar
 * button.
 */
public class PriceTrackingButtonController extends BaseButtonDataProvider {
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    /** Constructor. */
    public PriceTrackingButtonController(Context context, ObservableSupplier<Tab> tabSupplier,
            ModalDialogManager modalDialogManager, BottomSheetController bottomSheetController,
            Drawable buttonDrawable, Supplier<TabBookmarker> tabBookmarkerSupplier) {
        super(tabSupplier, modalDialogManager, buttonDrawable,
                context.getString(R.string.enable_price_tracking_menu_item),
                /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                /*supportsTinting=*/true, /*iphCommandBuilder*/ null,
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, /*tooltipTextResId*/ Resources.ID_NULL,
                /*showHoverHighlight*/ false);
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mBottomSheetController = bottomSheetController;

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState, int reason) {
                mButtonData.setEnabled(newState == SheetState.HIDDEN);
                notifyObservers(mButtonData.canShow());
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);
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
