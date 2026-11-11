// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.payments.Event2;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay when performing the
 * single payment app UI skip optimization in addition to preloading PaymentRequest even before the
 * "Buy" button is clicked.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    // Speed up the test by not looking up actual apps installed on the device.
    "disable-features=" + PaymentFeatureList.SERVICE_WORKER_PAYMENT_APPS,
})
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/376100658
public class PaymentRequestPaymentAppUiSkipPreloadTest {
    private static final long DISMISSED_TIMEOUT_SECONDS = 10L;

    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(
                    mCtaTestRule, "payment_request_bobpay_ui_skip_preload_test.html");

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mCtaTestRule).around(mPaymentRequestTestRule);

    @Before
    public void setUpPaymentRequestTestRule() throws Exception {
        mPaymentRequestTestRule.startOnTestPage();
    }

    /**
     * Clicks "buy" and waits for the skipped-UI payment flow to dismiss.
     *
     * <p>Uses a longer timeout than {@link CallbackHelper#WAIT_TIMEOUT_SECONDS}: on the Android 15
     * x64 emulator bots, tearing down the payment dialog's window can stall the UI thread for ~4.5s
     * (crbug.com/583928053), which together with the 500ms delay in bobpay_ui_skip_preload.js goes
     * over 5s.
     */
    private void clickBuyAndWaitForDismissed() throws TimeoutException {
        CallbackHelper dismissed = mPaymentRequestTestRule.getDismissed();
        int callCount = dismissed.getCallCount();
        mPaymentRequestTestRule.clickNode("buy");
        dismissed.waitForCallback(callCount, 1, DISMISSED_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        clickBuyAndWaitForDismissed();
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        clickBuyAndWaitForDismissed();
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, but responds immediately to
     * getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.SLOW_APP);
        clickBuyAndWaitForDismissed();
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, and responds slowly to
     * getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedSlowBobPay() throws TimeoutException {
        // buy() preloads a second PaymentRequest that is never shown and logs an
        // OtherAborted record when torn down, racing with assertExpected().
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.Events2",
                                Event2.REQUEST_METHOD_OTHER
                                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                                        | Event2.SKIPPED_SHOW
                                        | Event2.SELECTED_OTHER
                                        | Event2.PAY_CLICKED
                                        | Event2.COMPLETED)
                        .allowExtraRecords("PaymentRequest.Events2")
                        .build();

        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        clickBuyAndWaitForDismissed();
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
        watcher.assertExpected();
    }

    /** Two payments apps with the same payment method name should not skip payments UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoPaymentsAppsWithTheSamePaymentMethodName() throws TimeoutException {
        // buy() preloads a second PaymentRequest that is never shown and logs an
        // OtherAborted record when torn down, racing with assertExpected().
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.Events2",
                                Event2.REQUEST_METHOD_OTHER
                                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                                        | Event2.SHOWN
                                        | Event2.SELECTED_OTHER
                                        | Event2.PAY_CLICKED
                                        | Event2.COMPLETED)
                        .allowExtraRecords("PaymentRequest.Events2")
                        .build();

        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.FAST_APP);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.FAST_APP);
        mPaymentRequestTestRule.triggerUiAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
        watcher.assertExpected();
    }
}
