// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.MojoPaymentRequestGateKeeper;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentAppServiceBridge;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;

/** Creates an instance of PaymentRequest for use in Chrome. */
public class ChromePaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    // Tests can inject behaviour on future PaymentRequests via these objects.
    public static ChromePaymentRequestService.Delegate sDelegateForTest;
    @Nullable private static ChromePaymentRequestDelegateImplObserverForTest sObserverForTest;
    private final RenderFrameHost mRenderFrameHost;

    /** Observes the {@link ChromePaymentRequestDelegateImpl} for testing. */
    @VisibleForTesting
    /* package */ interface ChromePaymentRequestDelegateImplObserverForTest {
        /**
         * Called after an instance of {@link ChromePaymentRequestDelegateImpl} has just been
         * created.
         * @param delegateImpl The {@link ChromePaymentRequestDelegateImpl}.
         */
        void onCreatedChromePaymentRequestDelegateImpl(
                ChromePaymentRequestDelegateImpl delegateImpl);
    }

    /**
     * Production implementation of the ChromePaymentRequestService's Delegate. Gives true answers
     * about the system.
     */
    @VisibleForTesting
    public static class ChromePaymentRequestDelegateImpl
            implements ChromePaymentRequestService.Delegate {
        private final RenderFrameHost mRenderFrameHost;

        private ChromePaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService) {
            return new ChromePaymentRequestService(paymentRequestService, this);
        }

        @Override
        public boolean isOffTheRecord() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return true;
            Profile profile = Profile.fromWebContents(liveWebContents);
            if (profile == null) return true;
            return profile.isOffTheRecord();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return null;
            if (!OriginSecurityChecker.isSchemeCryptographic(
                    liveWebContents.getLastCommittedUrl())) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(liveWebContents);
        }

        @Override
        public boolean prefsCanMakePayment() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            return liveWebContents != null
                    && UserPrefs.get(Profile.fromWebContents(liveWebContents))
                            .getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Override
        public @Nullable String getTwaPackageName() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return null;
            Activity activity = ActivityUtils.getActivityFromWebContents(liveWebContents);
            if (!(activity instanceof CustomTabActivity)) return null;

            CustomTabActivity customTabActivity = ((CustomTabActivity) activity);
            if (!customTabActivity.isInTwaMode()) return null;
            return customTabActivity.getTwaPackage();
        }
    }

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public ChromePaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    /** Set an observer for the payment request service, cannot be null. */
    public static void setChromePaymentRequestDelegateImplObserverForTest(
            ChromePaymentRequestDelegateImplObserverForTest observer) {
        assert observer != null;
        sObserverForTest = observer;
    }

    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null) return new InvalidPaymentRequest();
        if (!mRenderFrameHost.isFeatureEnabled(PermissionsPolicyFeature.PAYMENT)) {
            mRenderFrameHost.terminateRendererDueToBadMessage(241 /*PAYMENTS_WITHOUT_PERMISSION*/);
            return null;
        }

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        ChromePaymentRequestService.Delegate delegate;
        if (sDelegateForTest != null) {
            delegate = sDelegateForTest;
        } else {
            ChromePaymentRequestDelegateImpl delegateImpl =
                    new ChromePaymentRequestDelegateImpl(mRenderFrameHost);
            if (sObserverForTest != null) {
                sObserverForTest.onCreatedChromePaymentRequestDelegateImpl(
                        /* delegateImpl= */ delegateImpl);
            }
            delegate = delegateImpl;
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();

        return new MojoPaymentRequestGateKeeper(
                (client, onClosed) ->
                        new PaymentRequestService(
                                mRenderFrameHost,
                                client,
                                onClosed,
                                delegate,
                                PaymentAppServiceBridge::new));
    }
}
