// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.test_support;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.mockito.Mockito;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.payments.ChromePaymentRequestService;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MojoPaymentRequestGateKeeper;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/** The builder of the PaymentRequest parameters. */
public class PaymentRequestParamsBuilder implements ChromePaymentRequestService.Delegate {
    private final PaymentRequestClient mClient;
    private final ChromePaymentRequestService.Delegate mDelegate;
    private final RenderFrameHost mRenderFrameHost;
    private final PaymentMethodData[] mMethodData;
    private final PaymentDetails mDetails;
    private final WebContents mWebContents;
    private final PaymentRequestSpec mSpec;
    private final PaymentUiService mPaymentUiService;
    private final PaymentOptions mOptions;
    private JourneyLogger mJourneyLogger;
    private String mSupportedMethod = "https://www.chromium.org";

    public static PaymentRequestParamsBuilder defaultBuilder(
            PaymentRequestClient client, PaymentUiService paymentUiService) {
        return new PaymentRequestParamsBuilder(client, paymentUiService);
    }

    public PaymentRequestParamsBuilder(
            PaymentRequestClient client, PaymentUiService paymentUiService) {
        mClient = client;
        mDelegate = this;
        mPaymentUiService = paymentUiService;
        mJourneyLogger = Mockito.mock(JourneyLogger.class);
        mWebContents = Mockito.mock(WebContents.class);
        Mockito.doReturn(JUnitTestGURLs.URL_1).when(mWebContents).getLastCommittedUrl();
        mRenderFrameHost = Mockito.mock(RenderFrameHost.class);
        // subframe
        Mockito.doReturn(JUnitTestGURLs.URL_2).when(mRenderFrameHost).getLastCommittedURL();
        Origin origin = Mockito.mock(Origin.class);
        Mockito.doReturn(origin).when(mRenderFrameHost).getLastCommittedOrigin();
        mMethodData = new PaymentMethodData[1];
        mDetails = new PaymentDetails();
        mDetails.id = "testId";
        mDetails.total = new PaymentItem();
        mOptions = new PaymentOptions();
        mSpec = Mockito.mock(PaymentRequestSpec.class);
    }

    public PaymentRequest buildAndInit() {
        mMethodData[0] = new PaymentMethodData();
        mMethodData[0].supportedMethod = mSupportedMethod;

        PaymentCurrencyAmount amount = new PaymentCurrencyAmount();
        amount.currency = "CNY";
        amount.value = "123";
        PaymentItem total = new PaymentItem();
        total.amount = amount;
        Mockito.doReturn(total).when(mSpec).getRawTotal();
        Map<String, PaymentMethodData> methodDataMap = new HashMap<>();
        methodDataMap.put(mMethodData[0].supportedMethod, mMethodData[0]);
        Mockito.doReturn(methodDataMap).when(mSpec).getMethodData();
        Mockito.doReturn(mOptions).when(mSpec).getPaymentOptions();

        PaymentRequest request =
                new MojoPaymentRequestGateKeeper(
                        (client, onClosed) ->
                                new PaymentRequestService(
                                        mRenderFrameHost, client, onClosed, this, () -> null));
        request.init(mClient, mMethodData, mDetails, mOptions);
        return request;
    }

    public PaymentRequestParamsBuilder setSupportedMethod(String supportedMethod) {
        mSupportedMethod = supportedMethod;
        return this;
    }

    public PaymentRequestParamsBuilder setRequestShipping(boolean requestShipping) {
        mOptions.requestShipping = requestShipping;
        return this;
    }

    public PaymentRequestParamsBuilder setJourneyLogger(JourneyLogger journeyLogger) {
        mJourneyLogger = journeyLogger;
        return this;
    }

    @Override
    public BrowserPaymentRequest createBrowserPaymentRequest(
            PaymentRequestService paymentRequestService) {
        return new ChromePaymentRequestService(paymentRequestService, mDelegate);
    }

    @Override
    public boolean isOffTheRecord() {
        return false;
    }

    @Override
    public String getInvalidSslCertificateErrorMessage() {
        return null;
    }

    @Override
    public boolean prefsCanMakePayment() {
        return false;
    }

    @Nullable
    @Override
    public String getTwaPackageName() {
        return null;
    }

    @Nullable
    @Override
    public WebContents getLiveWebContents(RenderFrameHost renderFrameHost) {
        return mWebContents;
    }

    @Override
    public boolean isOriginSecure(GURL url) {
        return true;
    }

    @Override
    public JourneyLogger createJourneyLogger(WebContents webContents) {
        return mJourneyLogger;
    }

    @Override
    public String formatUrlForSecurityDisplay(GURL uri) {
        return uri.getSpec();
    }

    @Override
    public byte[][] getCertificateChain(WebContents webContents) {
        return new byte[0][];
    }

    @Override
    public boolean isOriginAllowedToUseWebPaymentApis(GURL url) {
        return true;
    }

    @Override
    public boolean validatePaymentDetails(PaymentDetails details) {
        return true;
    }

    @Override
    public PaymentRequestSpec createPaymentRequestSpec(
            PaymentOptions options,
            PaymentDetails details,
            Collection<PaymentMethodData> methodData,
            String appLocale) {
        return mSpec;
    }

    @Override
    public PaymentUiService createPaymentUiService(
            PaymentUiService.Delegate delegate,
            PaymentRequestParams params,
            WebContents webContents,
            boolean isOffTheRecord,
            JourneyLogger journeyLogger,
            String topLevelOrigin) {
        return mPaymentUiService;
    }

    @Override
    public Activity getActivity(WebContents webContents) {
        Activity activity = Mockito.mock(Activity.class);
        Resources resources = Mockito.mock(Resources.class);
        Mockito.doReturn(resources).when(activity).getResources();
        return activity;
    }

    @Nullable
    @Override
    public TabModelSelector getTabModelSelector(WebContents webContents) {
        return Mockito.mock(TabModelSelector.class);
    }

    @Nullable
    @Override
    public TabModel getTabModel(WebContents webContents) {
        return Mockito.mock(TabModel.class);
    }

    @Nullable
    @Override
    public ActivityLifecycleDispatcher getActivityLifecycleDispatcher(WebContents webContents) {
        return Mockito.mock(ActivityLifecycleDispatcher.class);
    }

    @Override
    public PaymentAppFactoryInterface createAndroidPaymentAppFactory() {
        return null;
    }

    @Override
    public boolean isWebContentsActive(RenderFrameHost renderFrameHost) {
        return true;
    }

    @Override
    public WindowAndroid getWindowAndroid(RenderFrameHost renderFrameHost) {
        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        Context context = Mockito.mock(Context.class);
        WeakReference<Context> weakContext = Mockito.mock(WeakReference.class);
        Mockito.doReturn(context).when(weakContext).get();
        Mockito.doReturn(weakContext).when(window).getContext();
        return window;
    }

    @Override
    public PaymentHandlerHost createPaymentHandlerHost(
            WebContents webContents, PaymentRequestUpdateEventListener listener) {
        return Mockito.mock(PaymentHandlerHost.class);
    }
}
