// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for auto advancing to next field when typing in card numbers.
 *
 * Below valid test card numbers are from https://stripe.com/docs/testing#cards and
 * https://developers.braintreepayments.com/guides/unionpay/testing/javascript/v3
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestCardEditorAutoAdvanceTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // Set user has a shipping address and valid credit card on disk to make it easy to click in
        // to the payment section.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "1", "2050", "amex", R.drawable.amex_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test14DigitsCreditCard() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Diners credit card.
        final View focusedChildView = mPaymentRequestTestRule.getCardEditorFocusedView();
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3056 9309 0259 0"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '3056 9309 0259 00' is an invalid 14 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3056 9309 0259 00"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '3056 9309 0259 04' is a valid 14 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3056 9309 0259 04"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> focusedChildView.requestFocus());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3056 9309 0259 041"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test15DigitsCreditCard() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // American Express credit card.
        final View focusedChildView = mPaymentRequestTestRule.getCardEditorFocusedView();
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3782 822463 1000"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '3782 822463 10000' is an invalid 15 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3782 822463 10000"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '3782 822463 10005' is a valid 15 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"3782 822463 10005"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> focusedChildView.requestFocus());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"3782 822463 10005 1"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test16DigitsCreditCard() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // DISCOVER, JCB, MASTERCARD, MIR and VISA cards have 16 digits. Takes VISA as test input
        // which has 13 digits valid card.
        final View focusedChildView = mPaymentRequestTestRule.getCardEditorFocusedView();
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4012 8888 8888 "}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1' is a valid 13 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4012 8888 8888 1"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4012 8888 8888 188"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1880' is an invalid 16 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1880"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '4012 8888 8888 1881' is a valid 16 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1881"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> focusedChildView.requestFocus());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"4012 8888 8888 1881 1"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test19DigitsCreditCard() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // UNIONPAY credit card.
        final View focusedChildView = mPaymentRequestTestRule.getCardEditorFocusedView();
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"6250 9410 0652 859"}, mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '6250 9410 0652 8599' is a valid 16 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"6250 9410 0652 8599"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 00"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '6212 3456 7890 0000 001' is an invalid 19 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 001"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);

        // '6212 3456 7890 0000 003' is a valid 19 digits card number.
        mPaymentRequestTestRule.setTextInCardEditorAndWait(new String[] {"6212 3456 7890 0000 003"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() != focusedChildView);

        // Request focus to card number field after auto advancing above.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> focusedChildView.requestFocus());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"6212 3456 7890 0000 0031"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        Assert.assertTrue(mPaymentRequestTestRule.getCardEditorFocusedView() == focusedChildView);
    }
}
