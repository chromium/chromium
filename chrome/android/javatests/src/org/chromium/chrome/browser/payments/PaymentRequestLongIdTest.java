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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** A test to verify that PaymentRequest does not crash with very long request identifiers. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/376100658
public class PaymentRequestLongIdTest {
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(mCtaTestRule, "payment_request_long_id_test.html");

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
    public void testNoCrash() throws TimeoutException {
        mPaymentRequestTestRule.clickNode("buy");
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"ID cannot be longer than 1024 characters"});
    }
}
