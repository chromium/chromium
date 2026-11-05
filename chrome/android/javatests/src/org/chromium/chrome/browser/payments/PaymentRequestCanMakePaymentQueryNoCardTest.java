// Copyright 2016 The Chromium Authors
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for checking whether user can make a payment via either payment app or
 * a credit card. This user does not have a complete credit card on file.
 *
 * <p>TODO(crbug.com/40182225): Check if these tests are still relevant post basic-card removal.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/376100658
public class PaymentRequestCanMakePaymentQueryNoCardTest {
    // TODO(crbug.com/40743458): Batch this test. It currently fails when batched.
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(
                    mCtaTestRule, "payment_request_can_make_payment_query_test.html");

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mCtaTestRule).around(mPaymentRequestTestRule);

    @Before
    public void setUp() throws TimeoutException, InterruptedException {
        mPaymentRequestTestRule.startOnTestPage();
        // The user has an incomplete credit card on file. This is not sufficient for
        // canMakePayment() to return true.
        new AutofillTestHelper()
                .setCreditCard(
                        new CreditCard(
                                "",
                                /* isUserConfirmed= */ true,
                                true,
                                /* name= */ "",
                                "4111111111111111",
                                "1111",
                                "12",
                                "2050",
                                "visa",
                                R.drawable.visa_card,
                                /* billingAddressId= */ "",
                                /* serverId= */ ""));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoAppInFastBobPayInFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.NO_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "hasEnrolledInstrument",
                mPaymentRequestTestRule.getHasEnrolledInstrumentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"false"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoAppsInSlowBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.NO_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "hasEnrolledInstrument",
                mPaymentRequestTestRule.getHasEnrolledInstrumentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"false"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithFastBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithSlowBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});
    }
}
