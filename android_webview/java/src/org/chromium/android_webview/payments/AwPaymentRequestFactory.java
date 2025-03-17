// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.payments;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwContents;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.MojoPaymentRequestGateKeeper;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Creates an instance of PaymentRequest service for use by a frame running in WebView. The frame
 * must be live and active.
 */
public class AwPaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    private final RenderFrameHost mRenderFrameHost;

    private class AwPaymentRequestDelegate implements PaymentRequestService.Delegate {
        @Override
        public BrowserPaymentRequest createBrowserPaymentRequest(
                PaymentRequestService paymentRequestService) {
            return new AwPaymentRequestService(paymentRequestService);
        }

        @Override
        public boolean isOffTheRecord() {
            // WebView does not have a concept of "off the record" or "incognito" mode.
            return false;
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

        // TODO(crbug.com/403534114): Rename prefsCanMakePayment() to prefsHasEnrolledInstrument()
        // when this setting stops applying to both canMakePayment() and hasEnrolledInstrument().
        @Override
        public boolean prefsCanMakePayment() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) {
                return false;
            }

            // The prefsCanMakePayment() preference applies to both canMakePayment() and
            // hasEnrolledInstrument(). The https://crbug.com/403534114 is tracking removal of
            // canMakePayment() dependency on this preference.
            AwContents awContents = AwContents.fromWebContents(liveWebContents);
            return awContents != null && awContents.getSettings().getHasEnrolledInstrumentEnabled();
        }

        @Override
        @Nullable
        public String getTwaPackageName() {
            return null;
        }
    }

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public AwPaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    // InterfaceFactory<PaymentRequest>:
    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null
                || !mRenderFrameHost.isRenderFrameLive()
                || mRenderFrameHost.getLifecycleState() != LifecycleState.ACTIVE) {
            return new InvalidPaymentRequest();
        }

        if (!mRenderFrameHost.isFeatureEnabled(PermissionsPolicyFeature.PAYMENT)) {
            mRenderFrameHost.terminateRendererDueToBadMessage(241 /*PAYMENTS_WITHOUT_PERMISSION*/);
            return null;
        }

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) {
            return new InvalidPaymentRequest();
        }

        AwContents awContents = AwContents.fromWebContents(webContents);
        if (awContents == null || !awContents.getSettings().getPaymentRequestEnabled()) {
            return new InvalidPaymentRequest();
        }

        return new MojoPaymentRequestGateKeeper(
                (client, onClosed) ->
                        new PaymentRequestService(
                                mRenderFrameHost,
                                client,
                                onClosed,
                                new AwPaymentRequestDelegate(),
                                // Do not support payment apps implemented in C++, e.g., service
                                // workers and Secure Payment Confirmation (SPC), because these apps
                                // require UI:
                                /* paymentAppServiceBridgeSupplier= */ null));
    }
}
