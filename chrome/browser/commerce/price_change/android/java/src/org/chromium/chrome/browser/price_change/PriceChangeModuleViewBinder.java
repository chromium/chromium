// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** ViewBinder for the price change module. */
class PriceChangeModuleViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        PriceChangeModuleView moduleView = (PriceChangeModuleView) view;
        if (PriceChangeModuleProperties.MODULE_TITLE == propertyKey) {
            moduleView.setModuleTitle(model.get(PriceChangeModuleProperties.MODULE_TITLE));
        } else if (PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING == propertyKey) {
            moduleView.setProductTitle(
                    model.get(PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING));
        } else if (PriceChangeModuleProperties.MODULE_FAVICON_BITMAP == propertyKey) {
            moduleView.setFaviconImage(
                    model.get(PriceChangeModuleProperties.MODULE_FAVICON_BITMAP));
        } else if (PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING == propertyKey) {
            moduleView.setCurrentPrice(
                    model.get(PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING));
        } else if (PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING == propertyKey) {
            moduleView.setPreviousPrice(
                    model.get(PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING));
        } else if (PriceChangeModuleProperties.MODULE_DOMAIN_STRING == propertyKey) {
            moduleView.setPriceChangeDomain(
                    model.get(PriceChangeModuleProperties.MODULE_DOMAIN_STRING));
        } else if (PriceChangeModuleProperties.MODULE_PRODUCT_IMAGE_BITMAP == propertyKey) {
            moduleView.setProductImage(
                    model.get(PriceChangeModuleProperties.MODULE_PRODUCT_IMAGE_BITMAP));
        } else {
            assert false : "Unhandled property detected in PriceChangeModuleViewBinder!";
        }
    }
}
