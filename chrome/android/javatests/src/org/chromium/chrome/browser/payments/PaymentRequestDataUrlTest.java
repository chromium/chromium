// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** Web payments test for data URL.  */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestDataUrlTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "data:text/html,<html><head>"
                    + "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, "
                    + "maximum-scale=1\"></head><body><button id=\"buy\" onclick=\"try { "
                    + "(new PaymentRequest([{supportedMethods: 'basic-card'}], "
                    + "{total: {label: 'Total', "
                    + " amount: {currency: 'USD', value: '1.00'}}})).show(); "
                    + "} catch(e) { "
                    + "document.getElementById('result').innerHTML = e; "
                    + "}\">Data URL Test</button><div id='result'></div></body></html>",
            this);

    @Override
    public void onMainActivityStarted() {}

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNode("buy");
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"PaymentRequest is not defined"});
    }
}
