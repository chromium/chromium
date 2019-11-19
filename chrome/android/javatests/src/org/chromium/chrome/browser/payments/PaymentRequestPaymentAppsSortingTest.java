// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.support.test.filters.MediumTest;

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
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestPay;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/** A payment integration test that sorting payment apps and instruments by frecency. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppsSortingTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_alicepay_bobpay_charliepay_and_cards_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Visa card with complete set of information. This payment method is always listed
        // behind non-autofill payment instruments in payment request.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppsSortingByFrecency() throws TimeoutException {
        // Install a payment app with Bob Pay and Alice Pay, and another payment app with Charlie
        // Pay.
        final TestPay appA =
                new TestPay("https://alicepay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        final TestPay appB =
                new TestPay("https://bobpay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        final TestPay appC =
                new TestPay("https://charliepay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, mayCrawlUnused, callback) -> {
                    callback.onPaymentAppCreated(appA);
                    callback.onPaymentAppCreated(appB);
                    callback.onPaymentAppCreated(appC);
                    callback.onAllPaymentAppsCreated();
                });
        String alicePayId = appA.getAppIdentifier() + "https://alicepay.com";
        String bobPayId = appB.getAppIdentifier() + "https://bobpay.com";
        String charliePayId = appC.getAppIdentifier() + "https://charliepay.com";

        // The initial records for all payment methods are zeroes.
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(alicePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(bobPayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentUseCount(charliePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(alicePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(bobPayId));
        Assert.assertEquals(
                0, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(charliePayId));

        // Sets Alice Pay use count and use date to 5. Sets Bob Pay use count and use date to 10.
        // Sets Charlie Pay use count and use date to 15.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(alicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(alicePayId, 5);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(bobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(bobPayId, 10);
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(charliePayId, 15);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(charliePayId, 15);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Charlie Pay is listed at the first position.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(0));
        Assert.assertEquals(
                "https://bobpay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(1));
        Assert.assertEquals(
                "https://alicepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(2));
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1111<PDF>\nJoe Doe'.
        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601111\u202C\nJon Doe",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(3));

        // Cancel the Payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getDismissed());

        // Checks the records for all payment instruments haven't been changed.
        Assert.assertEquals(5, PaymentPreferencesUtil.getPaymentInstrumentUseCount(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(charliePayId));
        Assert.assertEquals(5, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(bobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(charliePayId));

        // Sets Alice Pay use count and use date to 20.
        PaymentPreferencesUtil.setPaymentInstrumentUseCountForTest(alicePayId, 20);
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(alicePayId, 20);

        mPaymentRequestTestRule.reTriggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Alice Pay is listed at the first position. Checks Bob Pay is listed at the second
        // position together with Alice Pay since they come from the same app.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals(
                "https://alicepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(0));
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(1));
        Assert.assertEquals(
                "https://bobpay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(2));
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1111<PDF>\nJoe Doe'.
        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601111\u202C\nJon Doe",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(3));

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Checks Alice Pay is selected as the default payment method.
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://alicepay.com", "\"transaction\"", "1337"});

        // Checks Alice Pay use count is increased by one after completing a payment request with
        // it.
        Assert.assertEquals(21, PaymentPreferencesUtil.getPaymentInstrumentUseCount(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentUseCount(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentInstrumentUseCount(charliePayId));
        Assert.assertTrue(PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(alicePayId) > 20);
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(bobPayId));
        Assert.assertEquals(
                15, PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(charliePayId));
    }
}
