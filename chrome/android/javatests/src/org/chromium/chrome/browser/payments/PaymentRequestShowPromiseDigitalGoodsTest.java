// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** A payment integration test for the show promise with digital goods. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseDigitalGoodsTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/digital_goods.html");

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDigitalGoodsFastApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mRule.executeJavaScriptAndWaitForResult("create('https://bobpay.test');");
        mRule.triggerUIAndWait("buy", mRule.getResultReady());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDigitalGoodsSlowApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        mRule.executeJavaScriptAndWaitForResult("create('https://bobpay.test');");
        mRule.triggerUIAndWait("buy", mRule.getResultReady());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipUIFastApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mRule.executeJavaScriptAndWaitForResult("create('https://bobpay.test');");
        mRule.clickNodeAndWait("buy", mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipUISlowApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        mRule.executeJavaScriptAndWaitForResult("create('https://bobpay.test');");
        mRule.clickNodeAndWait("buy", mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }
}
