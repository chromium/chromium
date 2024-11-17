// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.ui.base.IntentRequestTracker;

/**
 * Helper for detecting whether the device supports scanning credit cards and for scanning credit
 * cards. The default implementation cannot scan cards. An implementing subclass must provide a
 * factory that builds its instances.
 */
public class CreditCardScanner {
    /**
     * Can be used to build subclasses of the scanner without the user of the class knowing about
     * the subclass name.
     */
    private static Factory sFactory;

    /** The delegate to notify of scanning result. */
    protected final Delegate mDelegate;

    /** Builds instances of credit card scanners. */
    public interface Factory {
        /**
         * Builds an instance of credit card scanner.
         *
         * @param delegate The delegate to notify of scanning result.
         * @return An object that can scan a credit card.
         */
        CreditCardScanner create(Delegate delegate);
    }

    /** The delegate for credit card scanning. */
    public interface Delegate {
        /** Notifies the delegate that scanning was cancelled. */
        void onScanCancelled();

        /**
         * Notifies the delegate that scanning was successful.
         *
         * @param cardHolderName  The card holder name.
         * @param cardNumber      Credit card number.
         * @param expirationMonth Expiration month in the range [1, 12].
         * @param expirationYear  Expiration year, e.g. 2000.
         */
        void onScanCompleted(
                String cardHolderName, String cardNumber, int expirationMonth, int expirationYear);
    }

    /**
     * Sets the factory that can build instances of credit card scanners.
     *
     * @param factory Can build instances of credit card scanners.
     */
    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    /**
     * Creates an instance of a credit card scanner.
     *
     * @param delegate The delegate to notify of scanning result.
     * @return An object that can scan a credit card.
     */
    public static CreditCardScanner create(Delegate delegate) {
        return sFactory != null ? sFactory.create(delegate) : new CreditCardScanner(delegate);
    }

    /**
     * Constructor for the credit card scanner.
     *
     * @param delegate The delegate to notify of scanning result.
     */
    protected CreditCardScanner(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Returns true if this instance has the ability to scan credit cards.
     *
     * @return True if has ability to scan credit cards.
     */
    public boolean canScan() {
        return false;
    }

    /**
     * Scans a credit card. Will invoke a delegate callback with the result.
     *
     * @param tracker The intent request tracker used to show the scanner intent.
     */
    public void scan(IntentRequestTracker tracker) {
        mDelegate.onScanCancelled();
    }
}
