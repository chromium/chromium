// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestPay;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/** A payment integration test that sorting payment apps by frecency. */
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
        String billingAddressId = helper.setProfile(
                new AutofillProfile("", "https://example.com", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Visa card with complete set of information. This payment method is always listed
        // behind non-autofill payment apps in payment request.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppsSortingByFrecency() throws TimeoutException {
        // Install a payment app with Bob Pay and Alice Pay, and another payment app with Charlie
        // Pay.
        TestPay appA = new TestPay("https://alicepay.com", AppSpeed.FAST_APP);
        TestPay appB = new TestPay("https://bobpay.com", AppSpeed.FAST_APP);
        TestPay appC = new TestPay("https://charliepay.com", AppSpeed.FAST_APP);
        PaymentAppService.getInstance().addFactory(new PaymentAppFactoryInterface() {
            @Override
            public void create(PaymentAppFactoryDelegate delegate) {
                delegate.onCanMakePaymentCalculated(true);
                delegate.onPaymentAppCreated(appA);
                delegate.onPaymentAppCreated(appB);
                delegate.onPaymentAppCreated(appC);
                delegate.onDoneCreatingPaymentApps(/*factory=*/this);
            }
        });
        String alicePayId = appA.getIdentifier();
        String bobPayId = appB.getIdentifier();
        String charliePayId = appC.getIdentifier();

        // The initial records for all payment methods are zeroes.
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppUseCount(alicePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppUseCount(bobPayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppUseCount(charliePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppLastUseDate(alicePayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppLastUseDate(bobPayId));
        Assert.assertEquals(0, PaymentPreferencesUtil.getPaymentAppLastUseDate(charliePayId));

        // Sets Alice Pay use count and use date to 5. Sets Bob Pay use count and use date to 10.
        // Sets Charlie Pay use count and use date to 15.
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(alicePayId, 5);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(alicePayId, 5);
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(bobPayId, 10);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(bobPayId, 10);
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(charliePayId, 15);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(charliePayId, 15);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Charlie Pay is listed at the first position.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentAppLabel(0));
        Assert.assertEquals("https://bobpay.com", mPaymentRequestTestRule.getPaymentAppLabel(1));
        Assert.assertEquals("https://alicepay.com", mPaymentRequestTestRule.getPaymentAppLabel(2));
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1111<PDF>\nJoe Doe'.
        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601111\u202C\nJon Doe",
                mPaymentRequestTestRule.getPaymentAppLabel(3));

        // Cancel the Payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getDismissed());

        // Checks the records for all payment apps haven't been changed.
        Assert.assertEquals(5, PaymentPreferencesUtil.getPaymentAppUseCount(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentAppUseCount(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentAppUseCount(charliePayId));
        Assert.assertEquals(5, PaymentPreferencesUtil.getPaymentAppLastUseDate(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentAppLastUseDate(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentAppLastUseDate(charliePayId));

        // Sets Alice Pay use count and use date to 20.
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(alicePayId, 20);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(alicePayId, 20);

        mPaymentRequestTestRule.reTriggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Alice Pay is listed at the first position. Checks Bob Pay is listed at the second
        // position together with Alice Pay since they come from the same app.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());
        Assert.assertEquals("https://alicepay.com", mPaymentRequestTestRule.getPaymentAppLabel(0));
        Assert.assertEquals(
                "https://charliepay.com", mPaymentRequestTestRule.getPaymentAppLabel(1));
        Assert.assertEquals("https://bobpay.com", mPaymentRequestTestRule.getPaymentAppLabel(2));
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1111<PDF>\nJoe Doe'.
        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601111\u202C\nJon Doe",
                mPaymentRequestTestRule.getPaymentAppLabel(3));

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Checks Alice Pay is selected as the default payment method.
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://alicepay.com", "\"transaction\"", "1337"});

        // Checks Alice Pay use count is increased by one after completing a payment request with
        // it.
        Assert.assertEquals(21, PaymentPreferencesUtil.getPaymentAppUseCount(alicePayId));
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentAppUseCount(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentAppUseCount(charliePayId));
        Assert.assertTrue(PaymentPreferencesUtil.getPaymentAppLastUseDate(alicePayId) > 20);
        Assert.assertEquals(10, PaymentPreferencesUtil.getPaymentAppLastUseDate(bobPayId));
        Assert.assertEquals(15, PaymentPreferencesUtil.getPaymentAppLastUseDate(charliePayId));
    }
}
