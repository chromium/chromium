// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.function.Supplier;

/** Provides Discounts signal for showing contextual page action for a given tab. */
@NullMarked
public class DiscountsActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;

    public DiscountsActionProvider(Supplier<ShoppingService> shoppingServiceSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        if (tab == null || tab.getUrl() == null || !UrlUtilities.isHttpOrHttps(tab.getUrl())) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.DISCOUNTS, false);
            return;
        }

        ShoppingService shoppingService = mShoppingServiceSupplier.get();
        if (!shoppingService.isDiscountEligibleToShowOnNavigation()) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.DISCOUNTS, false);
            return;
        }

        shoppingService.getDiscountInfoForUrl(
                tab.getUrl(),
                (url, info) -> {
                    signalAccumulator.setSignal(
                            AdaptiveToolbarButtonVariant.DISCOUNTS, !assumeNonNull(info).isEmpty());
                });
    }
}
