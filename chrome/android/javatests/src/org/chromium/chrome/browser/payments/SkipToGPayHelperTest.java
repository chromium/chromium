// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.concurrent.TimeoutException;

/**
 * An integration test for determining eligibility of SkipToGPay experimental flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class SkipToGPayHelperTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    // A test PaymentMethodData[] shared by all test instances.
    private PaymentMethodData[] mTestMethodData;
    private AutofillTestHelper mHelper;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mHelper = new AutofillTestHelper();

        // Set up a test PaymentMethodData that requests "basic-card".
        mTestMethodData = new PaymentMethodData[1];
        mTestMethodData[0] = new PaymentMethodData();
        mTestMethodData[0].supportedMethod = MethodStrings.BASIC_CARD;
    }

    private AutofillProfile makeCompleteProfile() {
        return new AutofillProfile("", "https://example.com", true, "" /* honorific prefix */,
                "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "650-253-0000", "jon.doe@gmail.com", "en-US");
    }

    private CreditCard makeCreditCard(String billingAddressProfileId) {
        return new CreditCard("", "https://example.com", true, true, "Jon Doe", "4111111111111111",
                "1111", "12", "2050", "amex", R.drawable.amex_card, billingAddressProfileId,
                /*serverId=*/"");
    }

    /**
     * Helper function that runs canActivateExperiment() on the main thread and checks that it
     * returns |expected|.
     */
    void assertCanActivateExperiment(boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(expected,
                    SkipToGPayHelperUtil.canActivateExperiment(
                            mActivityTestRule.getWebContents(), mTestMethodData));
        });
    }

    /**
     * Verifies that when PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD is enabled, experiment is not
     * activiated if user has a complete autofill card.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY,
            "enable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD})
    public void
    testSkipToGPayIfNoCard_HasCard() throws TimeoutException {
        String billingAddressProfileId = mHelper.setProfile(makeCompleteProfile());
        mHelper.setCreditCard(makeCreditCard(billingAddressProfileId));
        assertCanActivateExperiment(false);
    }

    /**
     * Verifies that when PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD is enabled, experiment is
     * activated if user doesn't have a complete autofill card.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY,
            "enable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD})
    public void
    testSkipToGPayIfNoCard_IncompleteCard() throws TimeoutException {
        mHelper.setCreditCard(makeCreditCard(/*billingAddressProfileId=*/""));
        assertCanActivateExperiment(true);
    }

    /**
     * Verifies that when PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD is enabled, experiment is
     * activated if user doesn't have any autofill card.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY,
            "enable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD})
    public void
    testSkipToGPayIfNoCard_NoCard() throws TimeoutException {
        assertCanActivateExperiment(true);
    }

    /**
     * Verifies that when PAYMENT_REQUEST_SKIP_TO_GPAY is enabled, experiment is activated
     * regardless whether user has a complete autofill card or not.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"enable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY})
    public void testSkipToGPay_AlwaysEnabled() throws TimeoutException {
        // At this point, there is no card in the profile.
        assertCanActivateExperiment(true);

        // Add a credit card with a complete profile.
        String billingAddressProfileId = mHelper.setProfile(makeCompleteProfile());
        mHelper.setCreditCard(makeCreditCard(billingAddressProfileId));
        assertCanActivateExperiment(true);
    }

    /**
     * Verifies that when both experiment flags are disabled, experiment is not activated when user
     * doesn't have any autofill card.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY
            + "," + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD})
    public void
    testSkipToGPayDisabled_NoCard() throws TimeoutException {
        assertCanActivateExperiment(false);
    }

    /**
     * Verifies that when both experiment flags are disabled, experiment is not activated when user
     * has a complete autofill card.
     */
    @Test
    @SmallTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=" + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY
            + "," + PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD})
    public void
    testSkipToGPayDisabled_HasCard() throws TimeoutException {
        String billingAddressProfileId = mHelper.setProfile(makeCompleteProfile());
        mHelper.setCreditCard(makeCreditCard(billingAddressProfileId));
        assertCanActivateExperiment(false);
    }
}
