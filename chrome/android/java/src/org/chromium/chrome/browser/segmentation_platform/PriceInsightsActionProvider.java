// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides price insights signal for showing contextual page action for a given tab. */
public class PriceInsightsActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;

    public PriceInsightsActionProvider(Supplier<ShoppingService> shoppingServiceSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        if (tab == null || tab.getUrl() == null || !UrlUtilities.isHttpOrHttps(tab.getUrl())) {
            signalAccumulator.setHasPriceInsights(false);
            signalAccumulator.notifySignalAvailable();
            return;
        }

        ShoppingService shoppingService = mShoppingServiceSupplier.get();
        if (!shoppingService.isPriceInsightsEligible()) {
            signalAccumulator.setHasPriceInsights(false);
            signalAccumulator.notifySignalAvailable();
            return;
        }

        shoppingService.getPriceInsightsInfoForUrl(
                tab.getUrl(),
                (url, info) -> {
                    signalAccumulator.setHasPriceInsights(hasPriceInsightsInfoData(info));
                    signalAccumulator.notifySignalAvailable();
                });
    }

    private boolean hasPriceInsightsInfoData(PriceInsightsInfo info) {
        return info != null
                && !info.currencyCode.isEmpty()
                && info.catalogHistoryPrices != null
                && !info.catalogHistoryPrices.isEmpty()
                && info.jackpotUrl != null
                && !info.jackpotUrl.isEmpty();
    }
}
