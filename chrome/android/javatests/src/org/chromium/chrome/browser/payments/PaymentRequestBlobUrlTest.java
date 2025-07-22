// Copyright 2017 The Chromium Authors
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** Web payments test for blob URL. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestBlobUrlTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_blob_url_test.html");

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void test() throws TimeoutException {
        // Trigger the Blob URL load, and wait for it to finish.
        mPaymentRequestTestRule.clickNode("buy");
        mPaymentRequestTestRule.assertWaitForPageScaleFactorMatch(2);

        // Trigger the PaymentRequest, which should be rejected.
        mPaymentRequestTestRule.executeJavaScriptAndWaitForResult("triggerPaymentRequest();");

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"PaymentRequest is not defined"});
    }
}
