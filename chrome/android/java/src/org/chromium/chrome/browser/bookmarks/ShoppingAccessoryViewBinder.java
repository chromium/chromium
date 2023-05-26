// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ShoppingAccessoryView. */
class ShoppingAccessoryViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        ShoppingAccessoryView shoppingView = (ShoppingAccessoryView) view;
        if (key == ShoppingAccessoryViewProperties.PRICE_INFO) {
            shoppingView.setPriceInfo(model.get(ShoppingAccessoryViewProperties.PRICE_INFO));
        } else if (key == ShoppingAccessoryViewProperties.PRICE_TRACKED) {
            shoppingView.setPriceTracked(model.get(ShoppingAccessoryViewProperties.PRICE_TRACKED));
        }
    }
}
