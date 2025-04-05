// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** This class is responsible for providing UI resources for showing the Discounts action. */
public class DiscountsButtonController extends BaseButtonDataProvider {

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private @NonNull Supplier<CommerceBottomSheetContentController>
            mCommerceBottomSheetContentController;

    public DiscountsButtonController(
            Context context,
            Supplier<Tab> activeTabSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            @NonNull
                    Supplier<CommerceBottomSheetContentController>
                            commerceBottomSheetContentController) {
        super(
                activeTabSupplier,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_shoppingmode_24dp),
                context.getString(R.string.discount_container_title),
                R.string.discount_container_title,
                true,
                null,
                AdaptiveToolbarButtonVariant.DISCOUNTS,
                Resources.ID_NULL,
                false);

        mBottomSheetController = bottomSheetController;
        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        mButtonData.setEnabled(newState == SheetState.HIDDEN);
                        notifyObservers(mButtonData.canShow());
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);

        mCommerceBottomSheetContentController = commerceBottomSheetContentController;
    }

    @Override
    public void onClick(View view) {
        mCommerceBottomSheetContentController.get().requestShowContent();
    }

    @Override
    public void destroy() {
        super.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }
}
