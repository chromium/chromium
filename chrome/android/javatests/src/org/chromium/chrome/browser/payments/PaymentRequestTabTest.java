// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for dismissing the dialog when the user switches tabs or navigates
 * elsewhere.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestTabTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html");

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
                                .setPhoneNumber("555-555-5555")
                                .setEmailAddress("jon.doe@google.com")
                                .setLanguageCode("en-US")
                                .build());
        helper.setCreditCard(
                new CreditCard(
                        "",
                        "https://example.test",
                        true,
                        "Jon Doe",
                        "4111111111111111",
                        "1111",
                        "12",
                        "2050",
                        "visa",
                        R.drawable.visa_card,
                        billingAddressId,
                        /* serverId= */ ""));
    }

    /** If the user switches tabs somehow, the dialog is dismissed. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabSwitch() throws TimeoutException {
        // Install two apps to force showing the payment request UI.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () ->
                                mPaymentRequestTestRule
                                        .getActivity()
                                        .getTabCreator(false)
                                        .createNewTab(
                                                new LoadUrlParams("about:blank"),
                                                TabLaunchType.FROM_CHROME_UI,
                                                null));
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }

    /** If the user closes the tab, the dialog is dismissed. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabClose() throws TimeoutException {
        // Install two apps to force showing the payment request UI.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel currentModel =
                            mPaymentRequestTestRule.getActivity().getCurrentTabModel();
                    TabModelUtils.closeCurrentTab(currentModel);
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }

    /** If the user navigates anywhere, the dialog is dismissed. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabNavigate() throws TimeoutException {
        // Install two apps to force showing the payment request UI.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel currentModel =
                            mPaymentRequestTestRule.getActivity().getCurrentTabModel();
                    TabModelUtils.getCurrentTab(currentModel)
                            .loadUrl(new LoadUrlParams("about:blank"));
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }
}
