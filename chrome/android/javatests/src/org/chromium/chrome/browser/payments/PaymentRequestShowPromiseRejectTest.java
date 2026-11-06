// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** A payment integration test for the show promise that is rejected. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/376100658
public class PaymentRequestShowPromiseRejectTest {
    public final AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    public final PaymentRequestTestRule mRule =
            new PaymentRequestTestRule(mCtaTestRule, "show_promise/reject.html");

    @Rule public final RuleChain mRuleChain = RuleChain.outerRule(mCtaTestRule).around(mRule);

    @Before
    public void setUp() throws Exception {
        mRule.startOnTestPage();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testReject() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mRule.clickNodeAndWait("buy", mRule.getRendererClosedMojoConnection());
        mRule.expectResultContains(new String[] {"AbortError"});
    }
}
