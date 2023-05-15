// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Locale;

/** Business logic for the price-tracking chip accessory view. */
public class ShoppingAccessoryCoordinator {
    private final ShoppingAccessoryView mView;
    private final PropertyModel mModel;

    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param visual Whether the visual row should be used.
     */
    static ShoppingAccessoryView buildView(Context context) {
        ShoppingAccessoryView view = new ShoppingAccessoryView(context, null);
        view.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LayoutInflater.from(context).inflate(
                org.chromium.chrome.R.layout.shopping_accessory_view_layout, view);
        view.onFinishInflate();
        return view;
    }

    /**
     * Constructor for the shopping view coordinator.
     * @param context The calling context.
     * @param specifics The shopping specifics.
     */
    public ShoppingAccessoryCoordinator(Context context, ShoppingSpecifics specifics) {
        mView = ShoppingAccessoryCoordinator.buildView(context);

        mModel = new PropertyModel(ShoppingAccessoryViewProperties.ALL_KEYS);

        ProductPrice currentPrice = specifics.getCurrentPrice();
        ProductPrice previousPrice = specifics.getPreviousPrice();

        mModel.set(ShoppingAccessoryViewProperties.PRICE_INFO,
                new PriceInfo(previousPrice.getAmountMicros(), currentPrice.getAmountMicros(),
                        new CurrencyFormatter(
                                currentPrice.getCurrencyCode(), Locale.getDefault())));
        mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, specifics.getIsPriceTracked());
        PropertyModelChangeProcessor.create(mModel, mView, ShoppingAccessoryViewBinder::bind);
    }

    public PropertyModel getModel() {
        return mModel;
    }

    public View getView() {
        return mView;
    }
}
