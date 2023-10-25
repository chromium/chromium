// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
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
     *
     * @param context The calling context, usually the parent view.
     */
    static ShoppingAccessoryView buildView(Context context) {
        ShoppingAccessoryView view = new ShoppingAccessoryView(context, null);
        view.setLayoutParams(
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LayoutInflater.from(context).inflate(R.layout.shopping_accessory_view_layout, view);
        view.onFinishInflate();
        return view;
    }

    /**
     * Constructor for the shopping view coordinator.
     *
     * @param context The calling context.
     * @param specifics The shopping specifics.
     * @param shoppingService The shopping service to query price-trackability.
     */
    public ShoppingAccessoryCoordinator(
            Context context, ShoppingSpecifics specifics, ShoppingService shoppingService) {
        mView = ShoppingAccessoryCoordinator.buildView(context);
        mModel = new PropertyModel(ShoppingAccessoryViewProperties.ALL_KEYS);

        ProductPrice currentPrice = specifics.getCurrentPrice();
        ProductPrice previousPrice = specifics.getPreviousPrice();

        mModel.set(
                ShoppingAccessoryViewProperties.PRICE_INFO,
                new PriceInfo(
                        previousPrice.getAmountMicros(),
                        currentPrice.getAmountMicros(),
                        new CurrencyFormatter(
                                currentPrice.getCurrencyCode(), Locale.getDefault())));

        CommerceSubscription subscription =
                PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(specifics);
        mModel.set(
                ShoppingAccessoryViewProperties.PRICE_TRACKED,
                shoppingService.isSubscribedFromCache(subscription));

        PropertyModelChangeProcessor.create(mModel, mView, ShoppingAccessoryViewBinder::bind);
    }

    /** Returns the underlying property model. */
    public PropertyModel getModel() {
        return mModel;
    }

    /** Returns the view for display. */
    public View getView() {
        return mView;
    }

    /**
     * Sets the price-tracking state.
     *
     * @param enabled Whether price-tracking is enabled.
     */
    public void setPriceTrackingEnabled(boolean enabled) {
        mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, enabled);
    }
}
