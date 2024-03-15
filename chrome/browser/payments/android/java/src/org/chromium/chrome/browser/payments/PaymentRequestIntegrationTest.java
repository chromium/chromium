// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.payments.test_support.MockPaymentUiServiceBuilder;
import org.chromium.chrome.browser.payments.test_support.PaymentRequestParamsBuilder;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.components.payments.AndroidPaymentApp;
import org.chromium.components.payments.ErrorMessageUtil;
import org.chromium.components.payments.ErrorMessageUtilJni;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentApp.InstrumentDetailsCallback;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentMethodCategory;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestWebContentsData;
import org.chromium.components.payments.PaymentRequestWebContentsDataJni;
import org.chromium.components.payments.test_support.DefaultPaymentFeatureConfig;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImplJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A test for the integration of PaymentRequestService, MojoPaymentRequestGateKeeper,
 * ChromePaymentRequest and PaymentAppService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PaymentRequestIntegrationTest {
    private static final int NATIVE_WEB_CONTENTS_ANDROID = 1;
    private static final String STRINGIFIED_DETAILS = "test stringifiedDetails";
    private final ArgumentCaptor<InstrumentDetailsCallback> mPaymentAppCallbackCaptor =
            ArgumentCaptor.forClass(InstrumentDetailsCallback.class);
    private String mInstrumentMethodName = "https://www.chromium.org";
    private int mPaymentAppType = PaymentAppType.SERVICE_WORKER_APP;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ErrorMessageUtil.Natives mErrorMessageUtilMock;
    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Mock private Profile.Natives mProfileJniMock;
    @Mock private PaymentRequestWebContentsData.Natives mWebContentsDataJniMock;

    @Mock private PersonalDataManager mPersonalDataManager;

    private PaymentRequestClient mClient;
    private PaymentAppFactoryInterface mFactory;
    private PaymentApp mPaymentApp;
    private boolean mWaitForUpdatedDetails;
    private boolean mIsUserGestureShow;
    private PaymentRequestWebContentsData mPaymentRequestWebContentsData;

    @Before
    public void setUp() {
        mJniMocker.mock(WebContentsImplJni.TEST_HOOKS, mWebContentsJniMock);
        WebContentsImpl webContentsImpl =
                Mockito.spy(
                        WebContentsImpl.create(NATIVE_WEB_CONTENTS_ANDROID, mNavigationController));
        // We don't mock the WebContentsObserverProxy, so mock the observer behaviour.
        Mockito.doNothing().when(webContentsImpl).addObserver(Mockito.any());
        webContentsImpl.initializeForTesting();

        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);

        mPaymentRequestWebContentsData = new PaymentRequestWebContentsData(webContentsImpl);
        PaymentRequestWebContentsData.setInstanceForTesting(mPaymentRequestWebContentsData);

        mJniMocker.mock(PaymentRequestWebContentsDataJni.TEST_HOOKS, mWebContentsDataJniMock);
        Mockito.doNothing().when(mWebContentsDataJniMock).recordActivationlessShow(Mockito.any());
        Mockito.doReturn(false).when(mWebContentsDataJniMock).hadActivationlessShow(Mockito.any());

        mJniMocker.mock(ErrorMessageUtilJni.TEST_HOOKS, mErrorMessageUtilMock);
        Mockito.doAnswer(
                        args -> {
                            String[] methods = args.getArgument(0);
                            return "(Mock) Not supported error: " + Arrays.toString(methods);
                        })
                .when(mErrorMessageUtilMock)
                .getNotSupportedErrorMessage(Mockito.any());

        DefaultPaymentFeatureConfig.setDefaultFlagConfigurationForTesting();
        PaymentRequestService.resetShowingPaymentRequestForTest();
        PaymentAppService.getInstance().resetForTest();

        mClient = Mockito.mock(PaymentRequestClient.class);
    }

    @After
    public void tearDown() {
        PaymentRequestService.resetShowingPaymentRequestForTest();
        PaymentAppService.getInstance().resetForTest();
    }

    private PaymentApp mockPaymentApp() {
        PaymentApp app =
                mPaymentAppType == PaymentAppType.NATIVE_MOBILE_APP
                        ? Mockito.mock(AndroidPaymentApp.class)
                        : Mockito.mock(PaymentApp.class);
        Set<String> methodNames = new HashSet<>();
        methodNames.add(mInstrumentMethodName);
        Mockito.doReturn(methodNames).when(app).getInstrumentMethodNames();
        Mockito.doReturn(mPaymentAppType).when(app).getPaymentAppType();
        Mockito.doReturn("testPaymentApp").when(app).getIdentifier();
        Mockito.doReturn(true).when(app).handlesShippingAddress();
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
        ArgumentCaptor<PaymentResponse> responseCaptor =
                ArgumentCaptor.forClass(PaymentResponse.class);
        Mockito.verify(mClient, Mockito.times(1)).onPaymentResponse(responseCaptor.capture());
        PaymentResponse response = responseCaptor.getValue();
        Assert.assertNotNull(response);
        Assert.assertEquals(mInstrumentMethodName, response.methodName);
        Assert.assertEquals(STRINGIFIED_DETAILS, response.stringifiedDetails);
    }

    private PaymentRequestParamsBuilder defaultBuilder() {
        return defaultBuilder(defaultUiServiceBuilder().build()).setRequestShipping(true);
    }

    private MockPaymentUiServiceBuilder defaultUiServiceBuilder() {
        return MockPaymentUiServiceBuilder.defaultBuilder();
    }

    private PaymentRequestParamsBuilder defaultBuilder(PaymentUiService uiService) {
        mPaymentApp = mockPaymentApp();
        mFactory = Mockito.mock(PaymentAppFactoryInterface.class);
        Mockito.doAnswer(
                        (args) -> {
                            PaymentAppFactoryDelegate delegate = args.getArgument(0);
                            delegate.onCanMakePaymentCalculated(true);
                            delegate.onPaymentAppCreated(mPaymentApp);
                            delegate.onDoneCreatingPaymentApps(mFactory);
                            return null;
                        })
                .when(mFactory)
                .create(Mockito.any());
        PaymentRequestParamsBuilder builder =
                PaymentRequestParamsBuilder.defaultBuilder(mClient, uiService);
        PaymentAppService.getInstance().addUniqueFactory(mFactory, "testFactoryId");
        return builder;
    }

    private void show(PaymentRequest request) {
        request.show(mWaitForUpdatedDetails, mIsUserGestureShow);
    }

    private void assertInvokePaymentAppCalled() {
        Mockito.verify(mPaymentApp, Mockito.times(1))
                .invokePaymentApp(
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.anyString(),
                        Mockito.anyString(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.any(),
                        mPaymentAppCallbackCaptor.capture());
    }

    private void simulatePaymentAppRespond() {
        mPaymentAppCallbackCaptor
                .getValue()
                .onInstrumentDetailsReady(
                        mInstrumentMethodName, STRINGIFIED_DETAILS, new PayerData());
    }

    void setInstrumentMethodName(String methodName) {
        mInstrumentMethodName = methodName;
    }

    private void setAndroidPaymentApp() {
        mPaymentAppType = PaymentAppType.NATIVE_MOBILE_APP;
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
        PaymentRequest request =
                defaultBuilder(
                                defaultUiServiceBuilder()
                                        .setBuildPaymentRequestUIResult(
                                                "Error_BuildPaymentRequestUIResult")
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
        assertError(
                "(Mock) Not supported error: [https://www.chromium.org]",
                PaymentErrorReason.NOT_SUPPORTED);
    }

    @Test
    @Feature({"Payments"})
    public void testPlayBillingRequestAndSelectionAreLogged() {
        setInstrumentMethodName(MethodStrings.GOOGLE_PLAY_BILLING);
        setAndroidPaymentApp();
        JourneyLogger journeyLogger = Mockito.mock(JourneyLogger.class);
        PaymentRequest request =
                defaultBuilder()
                        .setJourneyLogger(journeyLogger)
                        .setSupportedMethod(MethodStrings.GOOGLE_PLAY_BILLING)
                        .buildAndInit();
        show(request);
        List<Integer> expectedMethods = new ArrayList<>();
        expectedMethods.add(PaymentMethodCategory.PLAY_BILLING);
        Mockito.verify(journeyLogger, Mockito.times(1))
                .setRequestedPaymentMethods(Mockito.eq(expectedMethods));
        Mockito.verify(journeyLogger, Mockito.times(1))
                .setSelectedMethod(Mockito.eq(PaymentMethodCategory.PLAY_BILLING));
    }

    @Test
    @Feature({"Payments"})
    public void testGooglePayAuthenticationRequestAndSelectionAreLogged() {
        setInstrumentMethodName(MethodStrings.GOOGLE_PAY_AUTHENTICATION);
        setAndroidPaymentApp();
        JourneyLogger journeyLogger = Mockito.mock(JourneyLogger.class);
        PaymentRequest request =
                defaultBuilder()
                        .setJourneyLogger(journeyLogger)
                        .setSupportedMethod(MethodStrings.GOOGLE_PAY_AUTHENTICATION)
                        .buildAndInit();
        show(request);
        List<Integer> expectedMethods = new ArrayList<>();
        expectedMethods.add(PaymentMethodCategory.GOOGLE_PAY_AUTHENTICATION);
        Mockito.verify(journeyLogger, Mockito.times(1))
                .setRequestedPaymentMethods(Mockito.eq(expectedMethods));
        Mockito.verify(journeyLogger, Mockito.times(1))
                .setSelectedMethod(Mockito.eq(PaymentMethodCategory.GOOGLE_PAY_AUTHENTICATION));
    }
}
