// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.FeaturePolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Creates an instance of PaymentRequest for use in Chrome.
 */
public class ChromePaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    // Tests can inject behaviour on future PaymentRequests via these objects.
    public static PaymentRequestService.Delegate sDelegateForTest;

    private final RenderFrameHost mRenderFrameHost;

    /**
     * Production implementation of the ChromePaymentRequestService's Delegate. Gives true answers
     * about the system.
     */
    public static class PaymentRequestDelegateImpl implements PaymentRequestService.Delegate {
        private final TwaPackageManagerDelegate mPackageManagerDelegate =
                new TwaPackageManagerDelegate();
        private final RenderFrameHost mRenderFrameHost;

        /* package */ PaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public boolean isOffTheRecord() {
            // TODO(crbug.com/1128658): Try getting around the Profile dependency, as in C++ where
            // we can do web_contents->GetBrowserContext()->IsOffTheRecord().
            WebContents liveWebContents = getLiveWebContents();
            if (liveWebContents == null) return true;
            Profile profile = Profile.fromWebContents(liveWebContents);
            if (profile == null) return true;
            return profile.isOffTheRecord();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents liveWebContents = getLiveWebContents();
            if (liveWebContents == null) return null;
            if (!OriginSecurityChecker.isSchemeCryptographic(
                        liveWebContents.getLastCommittedUrl())) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(liveWebContents);
        }

        @Override
        public boolean isWebContentsActive() {
            // TODO(crbug.com/1128658): Try making the WebContents inactive for instrumentation
            // tests rather than mocking it with this method.
            WebContents liveWebContents = getLiveWebContents();
            return liveWebContents != null && liveWebContents.getVisibility() == Visibility.VISIBLE;
        }

        @Override
        public boolean prefsCanMakePayment() {
            // TODO(crbug.com/1128658): Try replacing Profile with BrowserContextHandle, which
            // represents a Chrome Profile or WebLayer ProfileImpl, and which UserPrefs operates on.
            WebContents liveWebContents = getLiveWebContents();
            return liveWebContents != null
                    && UserPrefs.get(Profile.fromWebContents(liveWebContents))
                               .getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Override
        public boolean skipUiForBasicCard() {
            return false; // Only tests do this.
        }

        @Override
        @Nullable
        public String getTwaPackageName() {
            WebContents liveWebContents = getLiveWebContents();
            if (liveWebContents == null) return null;
            ChromeActivity activity = ChromeActivity.fromWebContents(liveWebContents);
            return activity != null ? mPackageManagerDelegate.getTwaPackageName(activity) : null;
        }

        @Nullable
        private WebContents getLiveWebContents() {
            WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
            return webContents != null && !webContents.isDestroyed() ? webContents : null;
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

    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null) return new InvalidPaymentRequest();
        if (!mRenderFrameHost.isFeatureEnabled(FeaturePolicyFeature.PAYMENT)) {
            mRenderFrameHost.getRemoteInterfaces().onConnectionError(
                    new MojoException(MojoResult.PERMISSION_DENIED));
            return null;
        }

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        PaymentRequestService.Delegate delegate;
        if (sDelegateForTest != null) {
            delegate = sDelegateForTest;
        } else {
            delegate = new PaymentRequestDelegateImpl(mRenderFrameHost);
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();

        return PaymentRequestService.createPaymentRequest(mRenderFrameHost,
                /*isOffTheRecord=*/delegate.isOffTheRecord(),
                /*skipUiForBasicCard=*/delegate.skipUiForBasicCard(), delegate,
                (paymentRequestService)
                        -> new ChromePaymentRequestService(paymentRequestService, delegate));
    }
}
