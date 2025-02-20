// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.payments;

import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.PaymentFeatureList;
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

        // TODO(crbug.com/381849264): Add WebView specific implementation for PaymentRequest.
        return new InvalidPaymentRequest();
    }
}
