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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.util.concurrent.TimeoutException;

/** Web payments test for blob URL. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestBlobUrlTest {
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(mCtaTestRule, "payment_request_blob_url_test.html");

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mCtaTestRule).around(mPaymentRequestTestRule);

    @Before
    public void setUpPaymentRequestTestRule() throws Exception {
        mPaymentRequestTestRule.startOnTestPage();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    @DisabledTest(message = "https://crbug.com/431047719")
    public void test() throws TimeoutException {
        // Trigger the Blob URL load, and wait for it to finish.
        mPaymentRequestTestRule.clickNode("buy");
        mCtaTestRule.assertWaitForPageScaleFactorMatch(2);

        // Trigger the PaymentRequest, which should be rejected.
        mPaymentRequestTestRule.executeJavaScriptAndWaitForResult("triggerPaymentRequest();");

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"PaymentRequest is not defined"});
    }
}
