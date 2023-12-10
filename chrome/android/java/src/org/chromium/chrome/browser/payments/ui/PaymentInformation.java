// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

/** The data to show in the PaymentRequest UI. */
public class PaymentInformation {
    private final ShoppingCart mShoppingCart;
    private final SectionInformation mShippingAddresses;
    private final SectionInformation mShippingOptions;
    private final SectionInformation mContactDetails;
    private final SectionInformation mPaymentMethods;

    /**
     * Builds the payment information to show in the PaymentRequest view.
     *
     * @param shoppingCart      The shopping cart.
     * @param shippingAddresses The shipping addresses.
     * @param shippingOptions   The shipping options.
     * @param contactDetails    The contact details.
     * @param paymentMethods    The payment methods.
     */
    public PaymentInformation(
            ShoppingCart shoppingCart,
            SectionInformation shippingAddresses,
            SectionInformation shippingOptions,
            SectionInformation contactDetails,
            SectionInformation paymentMethods) {
        mShoppingCart = shoppingCart;
        mShippingAddresses = shippingAddresses;
        mShippingOptions = shippingOptions;
        mContactDetails = contactDetails;
        mPaymentMethods = paymentMethods;
    }

    /**
     * Returns the shopping cart.
     *
     * @return The shopping cart.
     */
    public ShoppingCart getShoppingCart() {
        return mShoppingCart;
    }

    /**
     * Returns the shipping addresses.
     *
     * @return The shipping addresses.
     */
    public SectionInformation getShippingAddresses() {
        return mShippingAddresses;
    }

    /**
     * Returns the shipping options.
     *
     * @return The shipping options.
     */
    public SectionInformation getShippingOptions() {
        return mShippingOptions;
    }

    /**
     * Returns the contact details.
     *
     * @return The contact details.
     */
    public SectionInformation getContactDetails() {
        return mContactDetails;
    }

    /**
     * Returns the payment methods.
     *
     * @return The payment methods.
     */
    public SectionInformation getPaymentMethods() {
        return mPaymentMethods;
    }
}
