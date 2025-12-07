// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** This class is responsible for providing UI resources for showing the Discounts action. */
@NullMarked
public class DiscountsButtonController extends BaseButtonDataProvider {

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Supplier<@Nullable CommerceBottomSheetContentController>
            mCommerceBottomSheetContentController;

    public DiscountsButtonController(
            Context context,
            Supplier<@Nullable Tab> activeTabSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable CommerceBottomSheetContentController>
                    commerceBottomSheetContentController) {
        super(
                activeTabSupplier,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.ic_shoppingmode_24dp),
                /* contentDescription= */ context.getString(R.string.discount_container_title),
                /* actionChipLabelResId= */ R.string.discount_container_title,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.DISCOUNTS,
                /* tooltipTextResId= */ Resources.ID_NULL);

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
        if (mCommerceBottomSheetContentController.get() != null) {
            mCommerceBottomSheetContentController.get().requestShowContent();
        }
    }

    @Override
    public void destroy() {
        super.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }
}
