// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.payments.MockPaymentApp;
import org.chromium.components.payments.MockPaymentAppInstaller;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** Tests the PaymentRequest.canMakePayment() behavior with Android payment apps. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AndroidPaymentAppFinderCanMakePaymentTest {
    private static final String TEST_PAGE =
            "/components/test/data/payments/can_make_payment_and_show_checker.html";
    private static final String PAYMENT_METHOD_NAME = "https://payments.test/web-pay";

    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    private MockPaymentAppInstaller mMockPaymentAppInstaller = new MockPaymentAppInstaller();

    @Before
    public void setUp() throws Exception {
        mMockPaymentAppInstaller = new MockPaymentAppInstaller();
        mTestRule.start();
        mTestRule.loadUrl(mTestRule.getTestServer().getURL(TEST_PAGE));
    }

    @After
    public void tearDown() throws Exception {
        mMockPaymentAppInstaller.reset();
    }

    /**
     * Absence of payment apps should return "false" from canMakePayment(). This test case disables
     * the ALLOW_SHOW_WITHOUT_READY_TO_PAY feature.
     */
    @Test
    @MediumTest
    @DisableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCannotMakePaymentWithoutAppsAndShowProhibited() throws Exception {
        assertEquals("\"false\"", getCanMakePaymentResult());
        assertEquals("\"false\"", getHasEnrolledInstrumentResult());
        assertEquals("\"NotSupportedError\"", getShowResult());
    }

    /**
     * Absence of payment apps should return "false" from canMakePayment(). This test case enables
     * the ALLOW_SHOW_WITHOUT_READY_TO_PAY feature.
     */
    @Test
    @MediumTest
    @EnableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCannotMakePaymentWithoutAppsAndShowAllowed() throws Exception {
        assertEquals("\"false\"", getCanMakePaymentResult());
        assertEquals("\"false\"", getHasEnrolledInstrumentResult());
        assertEquals("\"NotSupportedError\"", getShowResult());
    }

    /**
     * When ALLOW_SHOW_WITHOUT_READY_TO_PAY feature is disabled and an Android payment app returns
     * "true" from the IS_READY_TO_PAY intent service, then PaymentRequest API returns "true" from
     * the canMakePayment() call.
     */
    @Test
    @MediumTest
    @DisableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCanMakePaymentWithAndroidPaymentAppReadyToPayAndShowProhibited()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).setReadyToPay(true).install();
        assertEquals("\"true\"", getCanMakePaymentResult());
        assertEquals("\"true\"", getHasEnrolledInstrumentResult());
        assertEquals("\"success\"", getShowResult());
    }

    /**
     * When ALLOW_SHOW_WITHOUT_READY_TO_PAY feature is disabled and an Android payment app returns
     * "false" from the IS_READY_TO_PAY intent service, then PaymentRequest API returns "false" from
     * the canMakePayment() call.
     */
    @Test
    @MediumTest
    @DisableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCannotMakePaymentWithAndroidPaymentAppNotReadyToPayAndShowProhibited()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).setReadyToPay(false).install();
        assertEquals("\"false\"", getCanMakePaymentResult());
        assertEquals("\"false\"", getHasEnrolledInstrumentResult());
        assertEquals("\"NotSupportedError\"", getShowResult());
    }

    /**
     * When ALLOW_SHOW_WITHOUT_READY_TO_PAY feature is enabled and an Android payment app returns
     * "true" from the IS_READY_TO_PAY intent service, then PaymentRequest API returns "true" from
     * the canMakePayment() call.
     */
    @Test
    @MediumTest
    @EnableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCanMakePaymentWithAndroidPaymentAppReadyToPayAndShowAllowed() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).setReadyToPay(true).install();
        assertEquals("\"true\"", getCanMakePaymentResult());
        assertEquals("\"true\"", getHasEnrolledInstrumentResult());
        assertEquals("\"success\"", getShowResult());
    }

    /**
     * When ALLOW_SHOW_WITHOUT_READY_TO_PAY feature is enabled and an Android payment app returns
     * "false" from the IS_READY_TO_PAY intent service, then PaymentRequest API returns "true" from
     * the canMakePayment() call.
     */
    @Test
    @MediumTest
    @EnableFeatures(PaymentFeatureList.ALLOW_SHOW_WITHOUT_READY_TO_PAY)
    public void testCanMakePaymentWithAndroidPaymentAppNotReadyToPayAndShowAllowed()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).setReadyToPay(false).install();
        assertEquals("\"true\"", getCanMakePaymentResult());
        assertEquals("\"false\"", getHasEnrolledInstrumentResult());
        assertEquals("\"success\"", getShowResult());
    }

    private String getCanMakePaymentResult() throws Exception {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                mTestRule.getWebContents(),
                String.format(
                        "initAndCheckCanMakePayment('%s').then(result => "
                                + "{window.domAutomationController.send(result);})",
                        PAYMENT_METHOD_NAME));
    }

    private String getHasEnrolledInstrumentResult() throws Exception {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                mTestRule.getWebContents(),
                "checkHasEnrolledInstrument().then(result => "
                        + "{window.domAutomationController.send(result);})");
    }

    private String getShowResult() throws Exception {
        return JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                mTestRule.getWebContents(),
                "checkShowResult().then(result => "
                        + "{window.domAutomationController.send(result);})");
    }

    private MockPaymentApp createPaymentApp() {
        return new MockPaymentApp()
                .setLabel("Test Payments App")
                .setPackage("test.payments.app")
                .setMethod(PAYMENT_METHOD_NAME)
                .setHasReadyToPayService(true)
                .setSignature("AABBCCDDEEFF001122334455")
                .setSha256CertificateFingerprint(
                        "79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE:06:50:CE:E4:75:80:62:65:02:FB:"
                                + "F6:F9:91:AB:6E:BE:21:7E");
    }
}
