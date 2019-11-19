// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Creates instances of PaymentRequest.
 */
public class PaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    // Tests can inject behaviour on future PaymentRequests via these objects.
    public static PaymentRequestImpl.Delegate sDelegateForTest;
    public static PaymentRequestImpl.NativeObserverForTest sNativeObserverForTest;

    private final RenderFrameHost mRenderFrameHost;

    /**
     * An implementation of PaymentRequest that immediately rejects all connections.
     * Necessary because Mojo does not handle null returned from createImpl().
     */
    private static final class InvalidPaymentRequest implements PaymentRequest {
        private PaymentRequestClient mClient;

        @Override
        public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
                PaymentDetails details, PaymentOptions options,
                boolean unusedGooglePayBridgeEligible) {
            mClient = client;
        }

        @Override
        public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
            if (mClient != null) {
                mClient.onError(
                        PaymentErrorReason.USER_CANCEL, ErrorStrings.WEB_PAYMENT_API_DISABLED);
                mClient.close();
            }
        }

        @Override
        public void updateWith(PaymentDetails details) {}

        @Override
        public void noUpdatedPaymentDetails() {}

        @Override
        public void abort() {}

        @Override
        public void complete(int result) {}

        @Override
        public void retry(PaymentValidationErrors errors) {}

        @Override
        public void canMakePayment() {
            if (mClient != null) {
                mClient.onCanMakePayment(CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);
            }
        }

        @Override
        public void hasEnrolledInstrument(boolean perMethodQuota) {
            if (mClient != null) {
                mClient.onHasEnrolledInstrument(
                        HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT);
            }
        }

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {}
    }

    /**
     * Production implementation of the PaymentRequestImpl's Delegate. Gives true answers
     * about the system.
     */
    public static class PaymentRequestDelegateImpl implements PaymentRequestImpl.Delegate {
        @Override
        public boolean isIncognito(@Nullable ChromeActivity activity) {
            return activity != null && activity.getCurrentTabModel().isIncognito();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage(WebContents webContents) {
            if (!OriginSecurityChecker.isSchemeCryptographic(webContents.getLastCommittedUrl()))
                return null;
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(webContents);
        }

        @Override
        public boolean isWebContentsActive(TabModel model, WebContents webContents) {
            return TabModelUtils.getCurrentWebContents(model) == webContents;
        }

        @Override
        public boolean prefsCanMakePayment() {
            return PrefServiceBridge.getInstance().getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Override
        public boolean skipUiForBasicCard() {
            return false; // Only tests do this.
        }
    }

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param webContents The web contents that may invoke the PaymentRequest API.
     */
    public PaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public PaymentRequest createImpl() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        if (mRenderFrameHost == null) return new InvalidPaymentRequest();

        PaymentRequestImpl.Delegate delegate;
        if (sDelegateForTest != null) {
            delegate = sDelegateForTest;
        } else {
            delegate = new PaymentRequestDelegateImpl();
        }
        return new PaymentRequestImpl(mRenderFrameHost, delegate, sNativeObserverForTest);
    }
}
