// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.payments;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwContents;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.components.payments.AndroidIntentLauncher;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.DialogController;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentAddressTypeConverter;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the WebView part of the PaymentRequest service. It runs in the process of the WebView
 * host app.
 *
 * <p>The methods annotated with "// No UI in WebView" comment are the methods that are intended for
 * updating the browser UI, but WebView does not have a UI for PaymentRequest API. The invoked
 * Android payment app is responsible for showing UI and communicating with the merchant website.
 */
/*package*/ class AwPaymentRequestService
        implements BrowserPaymentRequest, PaymentResponseHelperInterface, AndroidIntentLauncher {
    private static final String TAG = "AwPaymentRequest";
    // The following error strings are only used in WebView:
    private static final String RETRY_DISABLED = "PaymentResponse.retry() is disabled in WebView.";
    private static final String MORE_THAN_ONE_APP =
            "WebView supports launching only one payment app at a time.";

    @Nullable private PaymentRequestService mPaymentRequestService;
    private final List<PaymentApp> mApps = new ArrayList<>();

    // Whether the correct values for responses to the "pre-purchase queries"
    // (PaymentRequest.canMakePayment() and PaymentRequest.hasEnrolledInstrument()) are known. This
    // becomes true after all matching payment apps have been found and verified.
    private boolean mIsReadyToSendPrePurchaseQueryResponsesToRenderer;

    // The value for response to PaymentRequest.canMakePayment() as calculated by the
    // mPaymentRequestService, which contains the common (not specific to WebView) implementation of
    // the PaymentRequest service.
    private boolean mPaymentRequestCanMakePaymentResponse;

    // The value for response to PaymentRequest.hasEnrolledInstrument() as calculated by the
    // mPaymentRequestService, which contains the common (not specific to WebView) implementation of
    // the PaymentRequest service.
    private boolean mPaymentRequestHasEnrolledInstrumentResponse;

    // The method for sending the canMakePayment() response to the renderer process.
    @Nullable Callback<Boolean> mSenderOfCanMakePaymentResponseToRenderer;

    // The method for sending the hasEnrolledInstrument() response to the renderer process.
    @Nullable Callback<Boolean> mSenderOfHasEnrolledInstrumentResponseToRenderer;

    /**
     * Constructs the WebView part of the PaymentRequest service.
     *
     * @param paymentRequestService The common, core parts of the PaymentRequest implementation
     *     shared between Clank and WebView.
     */
    /*package*/ AwPaymentRequestService(PaymentRequestService paymentRequestService) {
        mPaymentRequestService = paymentRequestService;
    }

    // BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
        // No UI in WebView.
    }

    // BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        // No UI in WebView.
    }

    // BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
        // No UI in WebView.
        onCompleteHandled.run();
    }

    // BrowserPaymentRequest
    @Override
    public boolean disconnectIfNoRetrySupport() {
        // WebView implementation of PaymentRequest API does not support retrying payments, because
        // that requires either browser UI or payment handler support. However, WebView is not
        // supposed to have any UI and Android payment apps do not have support for retrying
        // payments.
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(
                    RETRY_DISABLED, PaymentErrorReason.NOT_SUPPORTED);
        }
        close();
        return true; // Indicate that the Mojo IPC connection has been closed.
    }

    // BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {
        // This code path cannot happen because disconnectIfNoRetrySupport() returned true.
        assert false;
    }

    // BrowserPaymentRequest:
    @Override
    public void close() {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.close();
            mPaymentRequestService = null;
        }
    }

    // BrowserPaymentRequest:
    @Override
    public void onSpecValidated(PaymentRequestSpec spec) {
        // No UI in WebView.
    }

    // BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        return !mApps.isEmpty();
    }

    // BrowserPaymentRequest:
    @Override
    @Nullable
    public String showOrSkipAppSelector(
            boolean isShowWaitingForUpdatedDetails,
            PaymentItem total,
            boolean shouldSkipAppSelector) {
        // No UI in WebView.
        return null;
    }

    // BrowserPaymentRequest:
    @Override
    public void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {
        mApps.addAll(pendingApps);
        mIsReadyToSendPrePurchaseQueryResponsesToRenderer = true;
        maybeSendPrePurchaseQueryResponsesToRendererIfReady();
    }

    // BrowserPaymentRequest:
    @Override
    @Nullable
    public String onShowCalledAndAppsQueriedAndDetailsFinalized() {
        if (mPaymentRequestService == null
                || mPaymentRequestService.getSpec() == null
                || mPaymentRequestService.getSpec().isDestroyed()) {
            return ErrorStrings.INVALID_STATE;
        }

        if (mApps.size() > 1) {
            // WebView does not have UI for the user to choose one of their multiple payment apps
            // that match merchant's PaymentRequest parameters. In this case, abort payment.
            Log.e(TAG, MORE_THAN_ONE_APP);
            return MORE_THAN_ONE_APP;
        }

        PaymentApp selectedPaymentApp = getSelectedPaymentApp();
        if (selectedPaymentApp == null) {
            Log.e(TAG, "No matching payment apps found.");
            return ErrorStrings.PAYMENT_APP_LAUNCH_FAIL;
        }

        mPaymentRequestService.getJourneyLogger().setSkippedShow(); // No browser UI was shown.
        mPaymentRequestService.invokePaymentApp(
                selectedPaymentApp, /* paymentResponseHelper= */ this);
        return null;
    }

    // BrowserPaymentRequest:
    @Override
    @Nullable
    public PaymentApp getSelectedPaymentApp() {
        return mApps.isEmpty() ? null : mApps.get(0);
    }

    // BrowserPaymentRequest:
    @Override
    public List<PaymentApp> getPaymentApps() {
        return mApps;
    }

    // BrowserPaymentRequest:
    @Override
    public boolean hasAnyCompleteApp() {
        return !mApps.isEmpty();
    }

    // BrowserPaymentRequest:
    @Override
    public DialogController getDialogController() {
        // AndroidPaymentApp uses this DialogController interface for showing debug message UI and
        // showing warning UI when launching payment apps from incognito mode. However, WebView is
        // not supposed to have any UI and does not have a concept of incognito mode.
        return null;
    }

    // BrowserPaymentRequest:
    @Override
    @Nullable
    public byte[][] getCertificateChain() {
        // WebView implementation of PaymentRequest does not pass the certificate chain of the
        // merchant website to the invoked Android payment app.
        return null;
    }

    // BrowserPaymentRequest:
    @Override
    public AndroidIntentLauncher getAndroidIntentLauncher() {
        return this;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isFullDelegationRequired() {
        // The payment app must provide shipping address and contact information, if a merchant
        // website requests it.
        return true;
    }

    // BrowserPaymentRequest:
    @Override
    public void maybeOverrideCanMakePaymentResponse(boolean response, Callback<Boolean> sender) {
        mPaymentRequestCanMakePaymentResponse = response;
        mSenderOfCanMakePaymentResponseToRenderer = sender;
        maybeSendPrePurchaseQueryResponsesToRendererIfReady();
    }

    // BrowserPaymentRequest:
    @Override
    public void maybeOverrideHasEnrolledInstrumentResponse(
            boolean response, Callback<Boolean> sender) {
        mPaymentRequestHasEnrolledInstrumentResponse = response;
        mSenderOfHasEnrolledInstrumentResponseToRenderer = sender;
        maybeSendPrePurchaseQueryResponsesToRendererIfReady();
    }

    /**
     * Sends the response for the JavaScript API "pre-purchase queries"
     * (PaymentRequest.canMakePayment() and PaymentRequest.hasEnrolledInstrument()) to the renderer,
     * if needed, while ensuring to send "false" if the number of matching payment apps is not
     * exactly equal to 1.
     */
    private void maybeSendPrePurchaseQueryResponsesToRendererIfReady() {
        if (!mIsReadyToSendPrePurchaseQueryResponsesToRenderer) {
            return;
        }

        boolean isExactlyOneApp = mApps.size() == 1;

        if (mSenderOfCanMakePaymentResponseToRenderer != null) {
            boolean result = mPaymentRequestCanMakePaymentResponse && isExactlyOneApp;
            if (!result) {
                Log.e(TAG, "Cannot make payments. Have %d apps.", mApps.size());
            }

            mSenderOfCanMakePaymentResponseToRenderer.onResult(result);
            mSenderOfCanMakePaymentResponseToRenderer = null;
        }

        if (mSenderOfHasEnrolledInstrumentResponseToRenderer != null) {
            boolean result = mPaymentRequestHasEnrolledInstrumentResponse && isExactlyOneApp;
            if (!result) {
                Log.e(TAG, "No enrolled instrument. Have %d apps.", mApps.size());
            }

            mSenderOfHasEnrolledInstrumentResponseToRenderer.onResult(result);
            mSenderOfHasEnrolledInstrumentResponseToRenderer = null;
        }
    }

    // PaymentResponseHelperInterface:
    @Override
    public void generatePaymentResponse(
            String methodName,
            String stringifiedDetails,
            PayerData payerData,
            PaymentResponseResultCallback resultCallback) {
        PaymentResponse response = new PaymentResponse();

        response.methodName = methodName;
        response.stringifiedDetails = stringifiedDetails;

        if (mPaymentRequestService.getPaymentOptions().requestShipping) {
            response.shippingAddress =
                    PaymentAddressTypeConverter.convertAddressToMojoPaymentAddress(
                            payerData.shippingAddress);
            response.shippingOption = payerData.selectedShippingOptionId;
        }

        response.payer = new PayerDetail();
        if (mPaymentRequestService.getPaymentOptions().requestPayerName) {
            response.payer.name = payerData.payerName;
        }
        if (mPaymentRequestService.getPaymentOptions().requestPayerPhone) {
            response.payer.phone = payerData.payerPhone;
        }
        if (mPaymentRequestService.getPaymentOptions().requestPayerEmail) {
            response.payer.email = payerData.payerEmail;
        }

        resultCallback.onPaymentResponseReady(response);
    }

    // AndroidIntentLauncher:
    @Override
    public void launchPaymentApp(
            Intent intent,
            Callback<String> errorCallback,
            WindowAndroid.IntentCallback intentCallback) {
        AwContents awContents = AwContents.fromWebContents(mPaymentRequestService.getWebContents());
        if (awContents != null) {
            awContents.startActivityForResult(intent, intentCallback);
        }
    }
}
