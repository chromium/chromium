// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.payments.ComponentPaymentRequestImpl;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.FeaturePolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
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
        public void onPaymentDetailsNotUpdated() {}

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
        public void hasEnrolledInstrument() {
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
        private final TwaPackageManagerDelegate mPackageManager = new TwaPackageManagerDelegate();
        private final RenderFrameHost mRenderFrameHost;

        /* package */ PaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public boolean isOffTheRecord(WebContents webContents) {
            // To be conservative, a request which we don't know its profile is considered
            // off-the-record, and thus user data would not be recorded in this case.
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            if (activity == null) return true;
            TabModel tabModel = activity.getCurrentTabModel();
            assert tabModel != null;
            Profile profile = tabModel.getProfile();
            if (profile == null) return true;
            return profile.isOffTheRecord();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents webContents = getWebContents();
            if (webContents == null || webContents.isDestroyed()) return null;
            if (!OriginSecurityChecker.isSchemeCryptographic(webContents.getLastCommittedUrl())) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(webContents);
        }

        @Override
        public boolean isWebContentsActive(@NonNull ChromeActivity activity) {
            return TabModelUtils.getCurrentWebContents(activity.getCurrentTabModel())
                    == getWebContents();
        }

        @Override
        public boolean prefsCanMakePayment() {
            WebContents webContents = getWebContents();
            return webContents != null && !webContents.isDestroyed()
                    && UserPrefs.get(Profile.fromWebContents(webContents))
                               .getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Override
        public boolean skipUiForBasicCard() {
            return false; // Only tests do this.
        }

        @Override
        @Nullable
        public String getTwaPackageName(@Nullable ChromeActivity activity) {
            return activity != null ? mPackageManager.getTwaPackageName(activity) : null;
        }

        @Nullable
        private WebContents getWebContents() {
            return WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        }
    }

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public PaymentRequestFactory(RenderFrameHost renderFrameHost) {
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

        PaymentRequestImpl.Delegate delegate;
        if (sDelegateForTest != null) {
            delegate = sDelegateForTest;
        } else {
            delegate = new PaymentRequestDelegateImpl(mRenderFrameHost);
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();

        return ComponentPaymentRequestImpl.createPaymentRequest(mRenderFrameHost,
                /*isOffTheRecord=*/delegate.isOffTheRecord(webContents),
                /*skipUiForBasicCard=*/delegate.skipUiForBasicCard(),
                (renderFrameHost, componentPaymentRequest, isOffTheRecord)
                        -> new PaymentRequestImpl(renderFrameHost, componentPaymentRequest,
                                isOffTheRecord, delegate));
    }
}
