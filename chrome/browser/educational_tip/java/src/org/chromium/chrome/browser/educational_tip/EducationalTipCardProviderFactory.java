// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupPromoCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A factory interface for building a EducationalTipCardProvider instance. */
public class EducationalTipCardProviderFactory {
    /**
     * @return An instance of EducationalTipCardProvider.
     */
    static EducationalTipCardProvider createInstance(
            @NonNull Context context,
            @EducationalTipCardType int cardType,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull BottomSheetController bottomSheetController) {
        if (cardType == EducationalTipCardType.DEFAULT_BROWSER_PROMO) {
            return new DefaultBrowserPromoCoordinator(
                    context, onModuleClickedCallback, bottomSheetController);
        }

        assert false : "Educational tip module's card type not supported!";
        return null;
    }

    /**
     * @return An instance of TabGroupPromoCoordinator.
     */
    static TabGroupPromoCoordinator createInstance(
            @NonNull Context context,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Runnable showTabSwitcherRunnable,
            @NonNull Supplier<ViewGroup> parentViewSupplier) {
        return new TabGroupPromoCoordinator(
                context,
                onModuleClickedCallback,
                modalDialogManagerSupplier,
                showTabSwitcherRunnable,
                parentViewSupplier);
    }
}
