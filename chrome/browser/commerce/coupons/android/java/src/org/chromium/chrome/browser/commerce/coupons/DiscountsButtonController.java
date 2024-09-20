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
import org.chromium.ui.modaldialog.ModalDialogManager;

/** This class is responsible for providing UI resources for showing the Discounts action. */
public class DiscountsButtonController extends BaseButtonDataProvider {

    private @NonNull Supplier<CommerceBottomSheetContentController>
            mCommerceBottomSheetContentController;

    public DiscountsButtonController(
            Context context,
            Supplier<Tab> activeTabSupplier,
            ModalDialogManager modalDialogManager,
            @NonNull
                    Supplier<CommerceBottomSheetContentController>
                            commerceBottomSheetContentController) {
        super(
                activeTabSupplier,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_shoppingmode_24dp),
                context.getString(R.string.discount_icon_expanded_text),
                R.string.discount_icon_expanded_text,
                true,
                null,
                AdaptiveToolbarButtonVariant.DISCOUNTS,
                Resources.ID_NULL,
                false);

        mCommerceBottomSheetContentController = commerceBottomSheetContentController;
    }

    @Override
    public void onClick(View view) {
        mCommerceBottomSheetContentController.get().requestShowContent();
    }
}
