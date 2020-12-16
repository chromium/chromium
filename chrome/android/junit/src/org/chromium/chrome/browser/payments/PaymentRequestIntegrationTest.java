// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.components.payments.ErrorMessageUtil;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentApp.InstrumentDetailsCallback;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;

import java.util.HashSet;
import java.util.Set;

/**
 * A test for the integration of PaymentRequestService, MojoPaymentRequestGateKeeper,
 * ChromePaymentRequest and PaymentAppService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {PaymentRequestIntegrationTest.ShadowErrorMessageUtil.class,
                ShadowPaymentFeatureList.class})
public class PaymentRequestIntegrationTest {
    private static final String METHOD_NAME = "https://www.chromium.org";
    private static final String STRINGIFIED_DETAILS = "test stringifiedDetails";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    private PaymentRequestClient mClient;
    private PaymentAppFactoryInterface mFactory;
    private PaymentApp mPaymentApp;
    private InstrumentDetailsCallback mPaymentAppCallback;
    private PaymentResponse mPaymentResponse;
    private boolean mIsUserGesture;
    private boolean mWaitForUpdatedDetails;

    /** The shadow of PaymentFeatureList. Not to use outside the test. */
    @Implements(ErrorMessageUtil.class)
    /* package */ static class ShadowErrorMessageUtil {
        @Implementation
        public static String getNotSupportedErrorMessage(Set<String> methods) {
            return "(Mock) Not supported error: " + methods.toString();
        }
    }

    @Before
    public void setUp() {
        ShadowPaymentFeatureList.setFeatureEnabled(
                PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP, true);
        PaymentRequestService.resetShowingPaymentRequestForTest();

        mClient = Mockito.mock(PaymentRequestClient.class);
        Mockito.doAnswer((args) -> {
                   mPaymentResponse = args.getArgument(0);
                   return null;
               })
                .when(mClient)
                .onPaymentResponse(Mockito.any());

        mPaymentApp = mockPaymentApp();
        mFactory = Mockito.mock(PaymentAppFactoryInterface.class);
        Mockito.doAnswer((args) -> {
                   PaymentAppFactoryDelegate delegate = args.getArgument(0);
                   delegate.onCanMakePaymentCalculated(true);
                   delegate.onPaymentAppCreated(mPaymentApp);
                   delegate.onDoneCreatingPaymentApps(mFactory);
                   return null;
               })
                .when(mFactory)
                .create(Mockito.any());
        PaymentAppService.getInstance().addFactory(mFactory);
    }

    @After
    public void tearDown() {
        PaymentRequestService.resetShowingPaymentRequestForTest();
    }

    private PaymentApp mockPaymentApp() {
        PaymentApp app = Mockito.mock(PaymentApp.class);
        Set<String> methodNames = new HashSet<>();
        methodNames.add(METHOD_NAME);
        Mockito.doReturn(methodNames).when(app).getInstrumentMethodNames();
        Mockito.doReturn(true).when(app).handlesShippingAddress();
        Mockito.doAnswer((args) -> {
                   mPaymentAppCallback = args.getArgument(11);
                   return null;
               })
                .when(app)
                .invokePaymentApp(Mockito.any(), Mockito.any(), Mockito.anyString(),
                        Mockito.anyString(), Mockito.any(), Mockito.any(), Mockito.any(),
                        Mockito.any(), Mockito.any(), Mockito.any(), Mockito.any(), Mockito.any());
        return app;
    }

    private void assertNoError() {
        Mockito.verify(mClient, Mockito.never()).onError(Mockito.anyInt(), Mockito.anyString());
    }

    private void assertError(String errorMessage, int paymentErrorReason) {
        Mockito.verify(mClient, Mockito.times(1))
                .onError(Mockito.eq(paymentErrorReason), Mockito.eq(errorMessage));
    }

    private void assertResponse() {
        Assert.assertNotNull(mPaymentResponse);
        Assert.assertEquals(METHOD_NAME, mPaymentResponse.methodName);
        Assert.assertEquals(STRINGIFIED_DETAILS, mPaymentResponse.stringifiedDetails);
    }

    private PaymentRequestParamsBuilder defaultBuilder() {
        return defaultBuilder(defaultUiServiceBuilder().build());
    }

    private MockPaymentUiServiceBuilder defaultUiServiceBuilder() {
        return MockPaymentUiServiceBuilder.defaultBuilder(mPaymentApp);
    }

    private PaymentRequestParamsBuilder defaultBuilder(PaymentUiService uiService) {
        PaymentRequestParamsBuilder builder =
                PaymentRequestParamsBuilder.defaultBuilder(mClient, uiService);
        PaymentAppService.getInstance().addUniqueFactory(mFactory, "testFactoryId");
        return builder;
    }

    private void show(PaymentRequest request) {
        request.show(mIsUserGesture, mWaitForUpdatedDetails);
    }

    private void assertInvokePaymentAppCalled() {
        Assert.assertNotNull(mPaymentAppCallback);
    }

    private void simulatePaymentAppRespond() {
        mPaymentAppCallback.onInstrumentDetailsReady(
                METHOD_NAME, STRINGIFIED_DETAILS, new PayerData());
    }

    @Test
    @Feature({"Payments"})
    public void testPaymentIsSuccessful() {
        PaymentRequest request = defaultBuilder().buildAndInit();
        Assert.assertNotNull(request);
        assertNoError();

        show(request);
        assertNoError();
        assertInvokePaymentAppCalled();

        simulatePaymentAppRespond();
        assertResponse();
    }

    @Test
    @Feature({"Payments"})
    public void testBuildPaymentRequestUiErrorFailsPayment() {
        PaymentRequest request = defaultBuilder(
                defaultUiServiceBuilder()
                        .setBuildPaymentRequestUIResult("Error_BuildPaymentRequestUIResult")
                        .build())
                                         .buildAndInit();
        assertNoError();

        show(request);
        assertError("Error_BuildPaymentRequestUIResult", PaymentErrorReason.NOT_SUPPORTED);
    }

    @Test
    @Feature({"Payments"})
    public void testCallHasNoAvailableAppsFailsPayment() {
        PaymentRequest request =
                defaultBuilder(defaultUiServiceBuilder().setHasAvailableApps(false).build())
                        .buildAndInit();
        assertNoError();

        show(request);
        assertError("(Mock) Not supported error: [https://www.chromium.org]",
                PaymentErrorReason.NOT_SUPPORTED);
    }
}