// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestPay;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;

import java.util.concurrent.TimeoutException;

/** A payment integration test that sorting payment apps by frecency. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppsSortingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_alicepay_bobpay_charliepay_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId =
                helper.setProfile(
                        AutofillProfile.builder()
                                .setFullName("Jon Doe")
                                .setCompanyName("Google")
                                .setStreetAddress("340 Main St")
                                .setRegion("CA")
                                .setLocality("Los Angeles")
                                .setPostalCode("90291")
                                .setCountryCode("US")
                                .setPhoneNumber("310-310-6000")
                                .setEmailAddress("jon.doe@gmail.com")
                                .setLanguageCode("en-US")
                                .build());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppsSortingByFrecency() throws TimeoutException {
        // Install a payment app with Bob Pay and Alice Pay, and another payment app with Charlie
        // Pay.
        TestPay appA = new TestPay("https://alicepay.test", AppSpeed.FAST_APP);
        TestPay appB = new TestPay("https://bobpay.test", AppSpeed.FAST_APP);
        TestPay appC = new TestPay("https://charliepay.test", AppSpeed.FAST_APP);
        PaymentAppService.getInstance()
                .addFactory(
                        new PaymentAppFactoryInterface() {
                            @Override
                            public void create(PaymentAppFactoryDelegate delegate) {
                                delegate.onCanMakePaymentCalculated(true);
                                delegate.onPaymentAppCreated(appA);
                                delegate.onPaymentAppCreated(appB);
                                delegate.onPaymentAppCreated(appC);
                                delegate.onDoneCreatingPaymentApps(/* factory= */ this);
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

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Charlie Pay is listed at the first position.
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentApps());
        Assert.assertEquals(
                "https://charliepay.test", mPaymentRequestTestRule.getPaymentAppLabel(0));
        Assert.assertEquals("https://bobpay.test", mPaymentRequestTestRule.getPaymentAppLabel(1));
        Assert.assertEquals("https://alicepay.test", mPaymentRequestTestRule.getPaymentAppLabel(2));

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

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Checks Alice Pay is listed at the first position. Checks Bob Pay is listed at the second
        // position together with Alice Pay since they come from the same app.
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentApps());
        Assert.assertEquals("https://alicepay.test", mPaymentRequestTestRule.getPaymentAppLabel(0));
        Assert.assertEquals(
                "https://charliepay.test", mPaymentRequestTestRule.getPaymentAppLabel(1));
        Assert.assertEquals("https://bobpay.test", mPaymentRequestTestRule.getPaymentAppLabel(2));

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Checks Alice Pay is selected as the default payment method.
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://alicepay.test", "\"transaction\"", "1337"});

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
