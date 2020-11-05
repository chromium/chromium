// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.CheckoutFunnelStep;
import org.chromium.components.payments.ErrorMessageUtil;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.Event;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.NotShownReason;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppFactoryParams;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentDetailsConverter;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.components.payments.PaymentUIsObserver;
import org.chromium.components.payments.PaymentValidator;
import org.chromium.components.payments.Section;
import org.chromium.components.payments.SkipToGPayHelper;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This is the Clank specific parts of {@link PaymentRequest}, with the parts shared with WebLayer
 * living in {@link PaymentRequestService}.
 */
public class ChromePaymentRequestService
        implements BrowserPaymentRequest, PaymentAppFactoryDelegate, PaymentAppFactoryParams,
                   PaymentRequestUpdateEventListener, PaymentApp.AbortCallback,
                   PaymentApp.InstrumentDetailsCallback,
                   PaymentResponseHelper.PaymentResponseRequesterDelegate,
                   PaymentDetailsConverter.MethodChecker, PaymentUiService.Delegate,
                   PaymentUIsObserver {
    private static final String TAG = "PaymentRequest";

    /**
     * Hold the currently showing PaymentRequest. Used to prevent showing more than one
     * PaymentRequest UI per browser process.
     */
    private static ChromePaymentRequestService sShowingPaymentRequest;

    // Null-check is necessary because retainers of ChromePaymentRequestService could still
    // reference ChromePaymentRequestService after mPaymentRequestService is set null, e.g.,
    // crbug.com/1122148.
    @Nullable
    private PaymentRequestService mPaymentRequestService;

    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final String mTopLevelOrigin;
    private final String mPaymentRequestOrigin;
    private final Origin mPaymentRequestSecurityOrigin;
    @Nullable
    private final byte[][] mCertificateChain;
    private final JourneyLogger mJourneyLogger;
    private final boolean mIsOffTheRecord;

    private final PaymentUiService mPaymentUiService;

    @Nullable
    private final PaymentOptions mPaymentOptions;
    private final boolean mRequestShipping;
    private boolean mWasRetryCalled;

    private boolean mHasClosed;

    private PaymentRequestSpec mSpec;
    private PaymentApp mInvokedPaymentApp;
    private boolean mHideServerAutofillCards;
    private boolean mWaitForUpdatedDetails;
    private PaymentHandlerHost mPaymentHandlerHost;

    /**
     * True when at least one url payment method identifier is specified in payment request.
     */
    private boolean mURLPaymentMethodIdentifiersSupported;

    /**
     * There are a few situations were the Payment Request can appear, from a code perspective, to
     * be shown more than once. This boolean is used to make sure it is only logged once.
     */
    private boolean mDidRecordShowEvent;

    /** The helper to create and fill the response to send to the merchant. */
    private PaymentResponseHelper mPaymentResponseHelper;

    /** If not empty, use this error message for rejecting PaymentRequest.show(). */
    private String mRejectShowErrorMessage;

    /** A helper to manage the Skip-to-GPay experimental flow. */
    private SkipToGPayHelper mSkipToGPayHelper;
    private boolean mIsGooglePayBridgeActivated;

    /** The delegate of this class */
    public interface Delegate extends PaymentRequestService.Delegate {
        /**
         * @return True if the UI can be skipped for "basic-card" scenarios. This will only ever be
         *         true in tests.
         */
        boolean skipUiForBasicCard();
    }

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param paymentRequestService The component side of the PaymentRequest implementation.
     * @param delegate The delegate of this class.
     */
    public ChromePaymentRequestService(
            PaymentRequestService paymentRequestService, Delegate delegate) {
        assert paymentRequestService != null;
        assert delegate != null;

        mPaymentRequestService = paymentRequestService;
        mRenderFrameHost = paymentRequestService.getRenderFrameHost();
        assert mRenderFrameHost != null;
        mPaymentRequestOrigin = paymentRequestService.getPaymentRequestOrigin();
        assert mPaymentRequestOrigin != null;
        mPaymentRequestSecurityOrigin = paymentRequestService.getPaymentRequestSecurityOrigin();
        assert mPaymentRequestSecurityOrigin != null;
        mTopLevelOrigin = paymentRequestService.getTopLevelOrigin();
        assert mTopLevelOrigin != null;
        mCertificateChain = paymentRequestService.getCertificateChain();
        mIsOffTheRecord = paymentRequestService.isOffTheRecord();
        mDelegate = delegate;
        mWebContents = paymentRequestService.getWebContents();
        mJourneyLogger = paymentRequestService.getJourneyLogger();

        mPaymentOptions = paymentRequestService.getPaymentOptions();
        assert mPaymentOptions != null;
        mRequestShipping = mPaymentOptions.requestShipping;

        mPaymentRequestService = paymentRequestService;
        mPaymentUiService = new PaymentUiService(/*delegate=*/this,
                /*params=*/this, mWebContents, mIsOffTheRecord, mJourneyLogger, mTopLevelOrigin,
                /*observer=*/this);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onWhetherGooglePayBridgeEligible(boolean googlePayBridgeEligible,
            WebContents webContents, PaymentMethodData[] rawMethodData) {
        mIsGooglePayBridgeActivated = googlePayBridgeEligible
                && SkipToGPayHelperUtil.canActivateExperiment(mWebContents, rawMethodData);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onSpecValidated(PaymentRequestSpec spec) {
        mSpec = spec;
        mPaymentUiService.initialize(
                mSpec.getPaymentDetails(), mSpec.getRawTotal(), mSpec.getRawLineItems());

        // Log the various types of payment methods that were requested by the merchant.
        boolean requestedMethodGoogle = false;
        // Not to record requestedMethodBasicCard because JourneyLogger ignore the case where the
        // specified networks are unsupported. mPaymentUiService.merchantSupportsAutofillCards()
        // better captures this group of interest than requestedMethodBasicCard.
        boolean requestedMethodOther = false;
        mURLPaymentMethodIdentifiersSupported = false;
        for (String methodName : mSpec.getMethodData().keySet()) {
            switch (methodName) {
                case MethodStrings.ANDROID_PAY:
                case MethodStrings.GOOGLE_PAY:
                    mURLPaymentMethodIdentifiersSupported = true;
                    requestedMethodGoogle = true;
                    break;
                case MethodStrings.BASIC_CARD:
                    // Not to record requestedMethodBasicCard because
                    // mPaymentUiService.merchantSupportsAutofillCards() is used instead.
                    break;
                default:
                    // "Other" includes https url, http url(when certifate check is bypassed) and
                    // the unlisted methods defined in {@link MethodStrings}.
                    requestedMethodOther = true;
                    if (methodName.startsWith(UrlConstants.HTTPS_URL_PREFIX)
                            || methodName.startsWith(UrlConstants.HTTP_URL_PREFIX)) {
                        mURLPaymentMethodIdentifiersSupported = true;
                    }
            }
        }
        mJourneyLogger.setRequestedPaymentMethodTypes(
                /*requestedBasicCard=*/mPaymentUiService.merchantSupportsAutofillCards(),
                requestedMethodGoogle, requestedMethodOther);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean disconnectIfExtraValidationFails(WebContents webContents,
            Map<String, PaymentMethodData> methodData, PaymentDetails details,
            PaymentOptions options) {
        assert mPaymentRequestService != null;
        assert methodData != null;
        assert details != null;

        if (mIsGooglePayBridgeActivated) {
            PaymentMethodData data = methodData.get(MethodStrings.GOOGLE_PAY);
            mSkipToGPayHelper = new SkipToGPayHelper(options, data.gpayBridgeData);
        }

        if (!parseAndValidateDetailsForSkipToGPayHelper(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return true;
        }
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onQueryForQuotaCreated(Map<String, PaymentMethodData> queryForQuota) {
        if (queryForQuota.containsKey(MethodStrings.BASIC_CARD)
                && PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            PaymentMethodData paymentMethodData = new PaymentMethodData();
            paymentMethodData.stringifiedData =
                    PaymentOptionsUtils.stringifyRequestedInformation(mPaymentOptions);
            queryForQuota.put("basic-card-payment-options", paymentMethodData);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void addPaymentAppFactories(PaymentAppService service) {
        String androidFactoryId = AndroidPaymentAppFactory.class.getName();
        if (!service.containsFactory(androidFactoryId)) {
            service.addUniqueFactory(new AndroidPaymentAppFactory(), androidFactoryId);
        }
        String swFactoryId = PaymentAppServiceBridge.class.getName();
        if (!service.containsFactory(swFactoryId)) {
            service.addUniqueFactory(new PaymentAppServiceBridge(), swFactoryId);
        }

        String autofillFactoryId = AutofillPaymentAppFactory.class.getName();
        if (!service.containsFactory(autofillFactoryId)) {
            service.addUniqueFactory(new AutofillPaymentAppFactory(), autofillFactoryId);
        }
        if (AutofillPaymentAppFactory.canMakePayments(mSpec.getMethodData())) {
            mPaymentUiService.setAutofillPaymentAppCreator(
                    AutofillPaymentAppFactory.createAppCreator(/*delegate=*/this));
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public PaymentAppFactoryDelegate getPaymentAppFactoryDelegate() {
        return this;
    }

    /** @return Whether the UI was built. */
    private boolean buildUI(ChromeActivity activity) {
        String error = mPaymentUiService.buildPaymentRequestUI(activity,
                /*isWebContentsActive=*/
                PaymentRequestServiceUtil.isWebContentsActive(mRenderFrameHost),
                /*waitForUpdatedDetails=*/mWaitForUpdatedDetails);
        if (error != null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(error);
            if (PaymentRequestService.getObserverForTest() != null) {
                PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
            }
            return false;
        }
        return true;
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called by the merchant website to show the payment request to the user.
     */
    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        if (mPaymentRequestService == null) return;

        if (mPaymentUiService.isShowingUI()) {
            // Can be triggered only by a compromised renderer. In normal operation, calling show()
            // twice on the same instance of PaymentRequest in JavaScript is rejected at the
            // renderer level.
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_SHOW_TWICE);
            return;
        }

        if (getIsAnyPaymentRequestShowing()) {
            // The renderer can create multiple instances of PaymentRequest and call show() on each
            // one. Only the first one will be shown. This also prevents multiple tabs and windows
            // from showing PaymentRequest UI at the same time.
            mJourneyLogger.setNotShown(NotShownReason.CONCURRENT_REQUESTS);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.ANOTHER_UI_SHOWING, PaymentErrorReason.ALREADY_SHOWING);
            if (PaymentRequestService.getObserverForTest() != null) {
                PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
            }
            return;
        }

        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.SHOW_CALLED);
        setShowingPaymentRequest(this);
        mPaymentRequestService.setCurrentPaymentRequestShowing(true);
        mPaymentRequestService.setUserGestureShow(isUserGesture);
        mWaitForUpdatedDetails = waitForUpdatedDetails;

        mJourneyLogger.setTriggerTime();
        if (mPaymentRequestService.disconnectIfNoPaymentMethodsSupported(hasAvailableApps())) {
            return;
        }
        if (mPaymentRequestService.isFinishedQueryingPaymentApps() && !showAppSelector()) return;

        triggerPaymentAppUiSkipIfApplicable();
    }

    /**
     * Shows the payment apps selector.
     * @return Whether the showing is successful.
     */
    // Implements BrowserPaymentRequest:
    @Override
    public boolean showAppSelector() {
        // Send AppListReady signal when all apps are created and request.show() is called.
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onAppListReady(
                    mPaymentUiService.getPaymentMethodsSection().getItems(), mSpec.getRawTotal());
        }
        // Calculate skip ui and build ui only after all payment apps are ready and
        // request.show() is called.
        mPaymentUiService.calculateWhetherShouldSkipShowingPaymentRequestUi(
                mPaymentRequestService.isUserGestureShow(), mURLPaymentMethodIdentifiersSupported,
                mDelegate.skipUiForBasicCard(), mPaymentOptions);
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (quitShowIfActivityNotFound(chromeActivity) || !buildUI(chromeActivity)) return false;
        if (!mPaymentUiService.shouldSkipShowingPaymentRequestUi() && mSkipToGPayHelper == null) {
            mPaymentUiService.getPaymentRequestUI().show();
        }
        return true;
    }

    private void dimBackgroundIfNotBottomSheetPaymentHandler(PaymentApp selectedApp) {
        // Putting isEnabled() last is intentional. It's to ensure not to confused the unexecuted
        // group and the disabled in A/B testing.
        if (selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP
                && PaymentHandlerCoordinator.isEnabled()) {
            // When the Payment Handler (PH) UI is based on Activity, dimming the Payment
            // Request (PR) UI does not dim the PH; when it's based on bottom-sheet, dimming
            // the PR dims both UIs. As bottom-sheet itself has dimming effect, dimming PR
            // is unnecessary for the bottom-sheet PH. For now, service worker based payment apps
            // are the only ones that can open the bottom-sheet.
            return;
        }
        mPaymentUiService.getPaymentRequestUI().dimBackground();
    }

    /** @return True if show() is quited. */
    private boolean quitShowIfActivityNotFound(@Nullable ChromeActivity chromeActivity) {
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (PaymentRequestService.getObserverForTest() != null) {
                PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
            }
            return true;
        }
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void triggerPaymentAppUiSkipIfApplicable() {
        // If we are skipping showing the Payment Request UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if ((mPaymentUiService.shouldSkipShowingPaymentRequestUi() || mSkipToGPayHelper != null)
                && mPaymentRequestService.isFinishedQueryingPaymentApps()
                && mPaymentRequestService.isCurrentPaymentRequestShowing()
                && !mWaitForUpdatedDetails) {
            assert !mPaymentUiService.getPaymentMethodsSection().isEmpty();
            assert mPaymentUiService.getPaymentRequestUI() != null;

            if (isMinimalUiApplicable()) {
                ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
                if (quitShowIfActivityNotFound(chromeActivity)) return;
                if (mPaymentUiService.triggerMinimalUI(chromeActivity, mSpec.getRawTotal(),
                            this::onMinimalUIReady, this::onMinimalUiConfirmed,
                            /*dismissObserver=*/
                            ()
                                    -> onUiAborted(AbortReason.ABORTED_BY_USER,
                                            ErrorStrings.USER_CANCELLED))) {
                    mDidRecordShowEvent = true;
                    mJourneyLogger.setEventOccurred(Event.SHOWN);
                } else {
                    disconnectFromClientWithDebugMessage(ErrorStrings.MINIMAL_UI_SUPPRESSED);
                }
                return;
            }

            assert !mPaymentUiService.getPaymentMethodsSection().isEmpty();
            PaymentApp selectedApp =
                    (PaymentApp) mPaymentUiService.getPaymentMethodsSection().getSelectedItem();
            dimBackgroundIfNotBottomSheetPaymentHandler(selectedApp);
            mDidRecordShowEvent = true;
            mJourneyLogger.setEventOccurred(Event.SKIPPED_SHOW);
            assert mSpec.getRawTotal() != null;
            // The total amount in details should be finalized at this point. So it is safe to
            // record the triggered transaction amount.
            assert !mWaitForUpdatedDetails;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
            invokePaymentApp(null /* selectedShippingAddress */, null /* selectedShippingOption */,
                    selectedApp);
        }
    }

    /** @return Whether the minimal UI should be shown. */
    private boolean isMinimalUiApplicable() {
        if (!mPaymentRequestService.isUserGestureShow()
                || mPaymentUiService.getPaymentMethodsSection() == null
                || mPaymentUiService.getPaymentMethodsSection().getSize() != 1) {
            return false;
        }

        PaymentApp app =
                (PaymentApp) mPaymentUiService.getPaymentMethodsSection().getSelectedItem();
        if (app == null || !app.isReadyForMinimalUI() || TextUtils.isEmpty(app.accountBalance())) {
            return false;
        }

        return PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_MINIMAL_UI);
    }

    private void onMinimalUIReady() {
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onMinimalUIReady();
        }
    }

    private void onMinimalUiConfirmed(PaymentApp app) {
        mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                mSpec.getRawTotal().amount.value, false /*completed*/);
        app.disableShowingOwnUI();
        invokePaymentApp(
                null /* selectedShippingAddress */, null /* selectedShippingOption */, app);
    }

    private void onMinimalUiCompletedAndClosed() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onComplete();
        close();
    }

    /** Called after the non-minimal UI has handled {@link #complete}. */
    private void onNonMinimalUiHandledComplete() {
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onCompleteCalled();
        }
        if (PaymentRequestService.getObserverForTest() != null) {
            PaymentRequestService.getObserverForTest().onCompleteReplied();
        }
        if (mPaymentRequestService != null) {
            mPaymentRequestService.onComplete();
            close();
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void modifyMethodData(@Nullable Map<String, PaymentMethodData> methodDataMap) {
        if (!mIsGooglePayBridgeActivated || methodDataMap == null) return;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (PaymentMethodData methodData : methodDataMap.values()) {
            String method = methodData.supportedMethod;
            assert !TextUtils.isEmpty(method);
            // If skip-to-GPay flow is activated, ignore all other payment methods, which can be
            // either "basic-card" or "https://android.com/pay". The latter is safe to ignore
            // because merchant has already requested Google Pay.
            if (!method.equals(MethodStrings.GOOGLE_PAY)) continue;
            if (methodData.gpayBridgeData != null
                    && !methodData.gpayBridgeData.stringifiedData.isEmpty()) {
                methodData.stringifiedData = methodData.gpayBridgeData.stringifiedData;
            }
            result.put(method, methodData);
        }
        methodDataMap.clear();
        methodDataMap.putAll(result);
    }

    /** Called by the payment app to get updated total based on the billing address, for example. */
    @Override
    public boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails) {
        if (TextUtils.isEmpty(methodName) || stringifiedDetails == null
                || mPaymentRequestService == null || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            return false;
        }

        mPaymentRequestService.onPaymentMethodChange(methodName, stringifiedDetails);
        return true;
    }

    /**
     * Called by the payment app to get updated payment details based on the shipping option.
     */
    @Override
    public boolean changeShippingOptionFromInvokedApp(String shippingOptionId) {
        if (TextUtils.isEmpty(shippingOptionId) || mPaymentRequestService == null
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || !mRequestShipping
                || mSpec.getRawShippingOptions() == null) {
            return false;
        }

        boolean isValidId = false;
        for (PaymentShippingOption option : mSpec.getRawShippingOptions()) {
            if (shippingOptionId.equals(option.id)) {
                isValidId = true;
                break;
            }
        }

        if (!isValidId) return false;

        mPaymentRequestService.onShippingOptionChange(shippingOptionId);
        return true;
    }

    /**
     * Called by payment app to get updated payment details based on the shipping address.
     */
    @Override
    public boolean changeShippingAddressFromInvokedApp(PaymentAddress shippingAddress) {
        if (shippingAddress == null || mPaymentRequestService == null || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || !mRequestShipping) {
            return false;
        }

        mPaymentRequestService.onShippingAddressChange(shippingAddress);
        return true;
    }

    /**
     * Get the WebContents of the Expandable Payment Handler for testing purpose; return null if
     * nonexistent.
     *
     * @return The WebContents of the Expandable Payment Handler.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static WebContents getPaymentHandlerWebContentsForTest() {
        if (sShowingPaymentRequest == null) return null;
        return sShowingPaymentRequest.getPaymentHandlerWebContentsForTestInternal();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    private WebContents getPaymentHandlerWebContentsForTestInternal() {
        return mPaymentUiService.getPaymentHandlerWebContentsForTest();
    }

    /**
     * Clicks the security icon of the Expandable Payment Handler for testing purpose; return false
     * if failed.
     *
     * @return Whether the click is successful.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean clickPaymentHandlerSecurityIconForTest() {
        if (sShowingPaymentRequest == null) return false;
        return sShowingPaymentRequest.clickPaymentHandlerSecurityIconForTestInternal();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    private boolean clickPaymentHandlerSecurityIconForTestInternal() {
        return mPaymentUiService.clickPaymentHandlerSecurityIconForTest();
    }

    /**
     * Simulates a click on the close button of the Payment Handler for testing purpose; return
     * false if failed.
     *
     * @return Whether the click is successful.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean clickPaymentHandlerCloseButtonForTest() {
        if (sShowingPaymentRequest == null) return false;
        return sShowingPaymentRequest.clickPaymentHandlerCloseButtonForTestInternal();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    private boolean clickPaymentHandlerCloseButtonForTestInternal() {
        return mPaymentUiService.clickPaymentHandlerCloseButtonForTest();
    }

    /**
     * Confirms payment in minimal UI. Used only in test.
     *
     * @return Whether the payment was confirmed successfully.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean confirmMinimalUIForTest() {
        return sShowingPaymentRequest != null
                && sShowingPaymentRequest.confirmMinimalUIForTestInternal();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    private boolean confirmMinimalUIForTestInternal() {
        return mPaymentUiService.confirmMinimalUIForTest();
    }

    /**
     * Dismisses the minimal UI. Used only in test.
     *
     * @return Whether the dismissal was successful.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean dismissMinimalUIForTest() {
        return sShowingPaymentRequest != null
                && sShowingPaymentRequest.dismissMinimalUIForTestInternal();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    private boolean dismissMinimalUIForTestInternal() {
        return mPaymentUiService.dismissMinimalUIForTest();
    }

    /**
     * Called to open a new PaymentHandler UI on the showing PaymentRequest.
     * @param url The url of the payment app to be displayed in the UI.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *         successful; null if failed.
     */
    @Nullable
    public static WebContents openPaymentHandlerWindow(GURL url) {
        if (sShowingPaymentRequest == null) return null;
        return sShowingPaymentRequest.openPaymentHandlerWindowInternal(url);
    }

    /**
     * Called to open a new PaymentHandler UI on this PaymentRequest.
     * @param url The url of the payment app to be displayed in the UI.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *         successful; null if failed.
     */
    @Nullable
    private WebContents openPaymentHandlerWindowInternal(GURL url) {
        assert mInvokedPaymentApp != null;
        assert mInvokedPaymentApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP;

        if (mPaymentRequestService == null) return null;

        @Nullable
        WebContents paymentHandlerWebContents = mPaymentUiService.showPaymentHandlerUI(
                url, mPaymentRequestService.isOffTheRecord());
        if (paymentHandlerWebContents != null) {
            ServiceWorkerPaymentAppBridge.onOpeningPaymentAppWindow(
                    /*paymentRequestWebContents=*/mWebContents,
                    /*paymentHandlerWebContents=*/paymentHandlerWebContents);

            // UKM for payment app origin should get recorded only when the origin of the invoked
            // payment app is shown to the user.
            mJourneyLogger.setPaymentAppUkmSourceId(mInvokedPaymentApp.getUkmSourceId());
        }
        return paymentHandlerWebContents;
    }

    @Override
    public boolean isInvokedInstrumentValidForPaymentMethodIdentifier(String methodName) {
        return mInvokedPaymentApp != null
                && mInvokedPaymentApp.isValidForPaymentMethodData(methodName, null);
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called by merchant to update the shipping options and line items after the user has selected
     * their shipping address or shipping option.
     */
    @Override
    public void updateWith(PaymentDetails details) {
        if (mPaymentRequestService == null) return;
        // mSpec.updateWith() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (mWaitForUpdatedDetails) {
            initializeWithUpdatedDetails(details);
            return;
        }

        if (mPaymentUiService.getPaymentRequestUI() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW);
            return;
        }

        if (!PaymentOptionsUtils.requestAnyInformation(mPaymentOptions)
                && (mInvokedPaymentApp == null
                        || !mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate())) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        if (!PaymentValidator.validatePaymentDetails(details)
                || !parseAndValidateDetailsForSkipToGPayHelper(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return;
        }
        mSpec.updateWith(details);
        mPaymentUiService.updateDetailsOnPaymentRequestUI(
                mSpec.getPaymentDetails(), mSpec.getRawTotal(), mSpec.getRawLineItems());

        if (mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            // After a payment app has been invoked, all of the merchant's calls to update the price
            // via updateWith() should be forwarded to the invoked app, so it can reflect the
            // updated price in its UI.
            mInvokedPaymentApp.updateWith(
                    PaymentDetailsConverter.convertToPaymentRequestDetailsUpdate(details,
                            mInvokedPaymentApp.handlesShippingAddress() /* handlesShipping */,
                            this /* methodChecker */));
            return;
        }

        if (mPaymentUiService.shouldShowShippingSection()
                && (mPaymentUiService.getUiShippingOptions().isEmpty()
                        || !TextUtils.isEmpty(details.error))
                && mPaymentUiService.getShippingAddressesSection().getSelectedItem() != null) {
            mPaymentUiService.getShippingAddressesSection().getSelectedItem().setInvalid();
            mPaymentUiService.getShippingAddressesSection().setSelectedItemIndex(
                    SectionInformation.INVALID_SELECTION);
            mPaymentUiService.getShippingAddressesSection().setErrorMessage(details.error);
        }

        boolean providedInformationToPaymentRequestUI =
                mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
    }

    private void initializeWithUpdatedDetails(PaymentDetails details) {
        assert mWaitForUpdatedDetails;
        // mSpec.updateWith() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        if (!PaymentValidator.validatePaymentDetails(details)
                || !parseAndValidateDetailsForSkipToGPayHelper(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return;
        }
        mSpec.updateWith(details);
        mPaymentUiService.updateDetailsOnPaymentRequestUI(
                mSpec.getPaymentDetails(), mSpec.getRawTotal(), mSpec.getRawLineItems());

        if (!TextUtils.isEmpty(details.error)) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        // Do not create shipping section When UI is not built yet. This happens when the show
        // promise gets resolved before all apps are ready.
        if (mPaymentUiService.getPaymentRequestUI() != null
                && mPaymentUiService.shouldShowShippingSection()) {
            mPaymentUiService.createShippingSectionForPaymentRequestUI(chromeActivity);
        }

        mWaitForUpdatedDetails = false;
        // Triggered tansaction amount gets recorded when both of the following conditions are met:
        // 1- Either Event.Shown or Event.SKIPPED_SHOW bits are set showing that transaction is
        // triggered (mDidRecordShowEvent == true). 2- The total amount in details won't change
        // (mWaitForUpdatedDetails == false). Record the transaction amount only when the triggered
        // condition is already met. Otherwise it will get recorded when triggered condition becomes
        // true.
        if (mDidRecordShowEvent) {
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
        }

        triggerPaymentAppUiSkipIfApplicable();

        if (mPaymentRequestService.isFinishedQueryingPaymentApps()
                && !mPaymentUiService.shouldSkipShowingPaymentRequestUi()) {
            boolean providedInformationToPaymentRequestUI =
                    mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
            if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
        }
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called when the merchant received a new shipping address, shipping option, or payment method
     * info, but did not update the payment details in response.
     */
    @Override
    public void onPaymentDetailsNotUpdated() {
        if (mPaymentRequestService == null) return;
        // mSpec.recomputeSpecForDetails(), mSpec.selectedShippingOptionError() can be used only
        // when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (mPaymentUiService.getPaymentRequestUI() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW);
            return;
        }

        mSpec.recomputeSpecForDetails();

        if (mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            mInvokedPaymentApp.onPaymentDetailsNotUpdated();
            return;
        }

        if (mPaymentUiService.shouldShowShippingSection()
                && (mPaymentUiService.getUiShippingOptions().isEmpty()
                        || !TextUtils.isEmpty(mSpec.selectedShippingOptionError()))
                && mPaymentUiService.getShippingAddressesSection().getSelectedItem() != null) {
            mPaymentUiService.getShippingAddressesSection().getSelectedItem().setInvalid();
            mPaymentUiService.getShippingAddressesSection().setSelectedItemIndex(
                    SectionInformation.INVALID_SELECTION);
            mPaymentUiService.getShippingAddressesSection().setErrorMessage(
                    mSpec.selectedShippingOptionError());
        }

        boolean providedInformationToPaymentRequestUI =
                mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
    }

    /**
     * If executing on the skip-to-gpay flow, do the flow's specific validation for details. If
     * valid, set shipping options for SkipToGPayHelper.
     * @param details The details specified by the merchant.
     * @return True if skip-to-gpay parameters are valid or when skip-to-gpay does not apply.
     */
    private boolean parseAndValidateDetailsForSkipToGPayHelper(PaymentDetails details) {
        return mSkipToGPayHelper == null || mSkipToGPayHelper.setShippingOptionIfValid(details);
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean waitForUpdatedDetails() {
        return mWaitForUpdatedDetails;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void recordShowEventAndTransactionAmount() {
        if (mDidRecordShowEvent) return;
        mDidRecordShowEvent = true;
        mJourneyLogger.setEventOccurred(Event.SHOWN);
        // Record the triggered transaction amount only when the total amount in details is
        // finalized (i.e. mWaitForUpdatedDetails == false). Otherwise it will get recorded when
        // the updated details become available.
        if (!mWaitForUpdatedDetails) {
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
        }
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    @Override
    public void onInstrumentDetailsLoadingWithoutUI() {
        if (mPaymentRequestService == null || mPaymentUiService.getPaymentRequestUI() == null
                || mPaymentResponseHelper == null) {
            return;
        }

        assert mPaymentUiService.getSelectedPaymentAppType() == PaymentAppType.AUTOFILL;

        mPaymentUiService.getPaymentRequestUI().showProcessingMessage();
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean invokePaymentApp(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, PaymentApp selectedPaymentApp) {
        mInvokedPaymentApp = selectedPaymentApp;

        EditableOption selectedContact = mPaymentUiService.getContactSection() != null
                ? mPaymentUiService.getContactSection().getSelectedItem()
                : null;
        mPaymentResponseHelper = new PaymentResponseHelper(selectedShippingAddress,
                selectedShippingOption, selectedContact, mInvokedPaymentApp, mPaymentOptions,
                mSkipToGPayHelper != null, this);

        mInvokedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        // Only native apps can use PaymentDetailsUpdateService.
        if (mInvokedPaymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance().initialize(new PackageManagerDelegate(),
                    ((AndroidPaymentApp) mInvokedPaymentApp).packageName(),
                    this /* PaymentApp.PaymentRequestUpdateEventListener */);
        }
        mPaymentRequestService.invokePaymentApp(mInvokedPaymentApp, /*callback=*/this);
        return !mInvokedPaymentApp.isAutofillInstrument();
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost = new PaymentHandlerHost(mWebContents, /*delegate=*/this);
        }
        return mPaymentHandlerHost;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean wasRetryCalled() {
        return mWasRetryCalled;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void onUiAborted(@AbortReason int reason, String debugMessage) {
        mJourneyLogger.setAborted(reason);
        disconnectFromClientWithDebugMessage(debugMessage);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    // Implement BrowserPaymentRequest:
    // This method is not supposed to be used outside this class and
    // PaymentRequestService.
    @Override
    public void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mPaymentRequestService != null) {
            mPaymentRequestService.onError(reason, debugMessage);
        }
        close();
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onConnectionTerminated();
        }
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {
        if (mPaymentRequestService == null) return;

        if (mInvokedPaymentApp != null) {
            mInvokedPaymentApp.abortPaymentApp(/*callback=*/this);
            return;
        }
        onInstrumentAbortResult(true);
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    /** Called by the payment app in response to an abort request. */
    @Override
    public void onInstrumentAbortResult(boolean abortSucceeded) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onAbort(abortSucceeded);
        if (abortSucceeded) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_MERCHANT);
            close();
        } else {
            if (PaymentRequestService.getObserverForTest() != null) {
                PaymentRequestService.getObserverForTest().onPaymentRequestServiceUnableToAbort();
            }
        }
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onAbortCalled();
        }
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(int result) {
        if (mPaymentRequestService == null) return;

        if (result != PaymentComplete.FAIL) {
            mJourneyLogger.setCompleted();
            if (!PaymentPreferencesUtil.isPaymentCompleteOnce()) {
                PaymentPreferencesUtil.setPaymentCompleteOnce();
            }
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, true /*completed*/);
        }

        mPaymentUiService.onPaymentRequestComplete(result,
                /*onMinimalUiErroredAndClosed=*/this::close, this::onMinimalUiCompletedAndClosed,
                this::onNonMinimalUiHandledComplete);
    }

    // Implement BrowserPaymentRequest:
    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mPaymentRequestService == null) return;
        // mSpec.retry() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (!PaymentValidator.validatePaymentValidationErrors(errors)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_VALIDATION_ERRORS);
            return;
        }
        mSpec.retry(errors);

        mWasRetryCalled = true;

        mPaymentUiService.onRetry(errors);
    }

    // Implement BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        assert mPaymentRequestService != null;
        mPaymentRequestService.close();
        mPaymentRequestService = null;

        closeUIAndDestroyNativeObjects();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean hasClosed() {
        return mHasClosed;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentMethodData> getMethodData() {
        // GetMethodData should not get called after PR is closed.
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getMethodData();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getId() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getId();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getTopLevelOrigin() {
        return mTopLevelOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getPaymentRequestOrigin() {
        return mPaymentRequestOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Origin getPaymentRequestSecurityOrigin() {
        return mPaymentRequestSecurityOrigin;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    @Nullable
    public byte[][] getCertificateChain() {
        return mCertificateChain;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentDetailsModifier> getUnmodifiableModifiers() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return Collections.unmodifiableMap(mSpec.getModifiers());
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentItem getRawTotal() {
        assert !mHasClosed;
        assert !mSpec.isDestroyed();
        return mSpec.getRawTotal();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean getMayCrawl() {
        return !mPaymentUiService.canUserAddCreditCard()
                || PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.WEB_PAYMENTS_ALWAYS_ALLOW_JUST_IN_TIME_PAYMENT_APP);
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentRequestUpdateEventListener getPaymentRequestUpdateEventListener() {
        return this;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentOptions getPaymentOptions() {
        return mPaymentOptions;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public PaymentRequestSpec getSpec() {
        return mSpec;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    @Nullable
    public String getTwaPackageName() {
        return mDelegate.getTwaPackageName();
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public PaymentAppFactoryParams getParams() {
        return this;
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onCanMakePaymentCalculated(boolean canMakePayment) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onCanMakePaymentCalculated(canMakePayment);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        if (mPaymentRequestService == null) return;

        mHideServerAutofillCards |= paymentApp.isServerAutofillInstrumentReplacement();
        paymentApp.setHaveRequestedAutofillData(mPaymentUiService.haveRequestedAutofillData());
        mPaymentRequestService.onPaymentAppCreated(paymentApp);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreationError(String errorMessage) {
        if (TextUtils.isEmpty(mRejectShowErrorMessage)) mRejectShowErrorMessage = errorMessage;
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory /* Unused */) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onDoneCreatingPaymentApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps) {
        if (mHideServerAutofillCards) {
            List<PaymentApp> nonServerAutofillCards = new ArrayList<>();
            int numberOfPendingApps = pendingApps.size();
            for (int i = 0; i < numberOfPendingApps; i++) {
                if (!pendingApps.get(i).isServerAutofillInstrument()) {
                    nonServerAutofillCards.add(pendingApps.get(i));
                }
            }
            pendingApps = nonServerAutofillCards;
        }

        // Load the validation rules for each unique region code in the credit card billing
        // addresses and check for validity.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < pendingApps.size(); ++i) {
            @Nullable
            String countryCode = pendingApps.get(i).getCountryCode();
            if (countryCode != null && !uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForAddressNormalization(countryCode);
            }
        }

        mPaymentUiService.rankPaymentAppsForPaymentRequestUI(pendingApps);

        // Possibly pre-select the first app on the list.
        int selection = !pendingApps.isEmpty() && pendingApps.get(0).canPreselect()
                ? 0
                : SectionInformation.NO_SELECTION;

        // The list of payment apps is ready to display.
        mPaymentUiService.setPaymentMethodsSection(
                new SectionInformation(PaymentRequestUI.DataType.PAYMENT_METHODS, selection,
                        new ArrayList<>(pendingApps)));

        // Record the number suggested payment methods and whether at least one of them was
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(Section.PAYMENT_METHOD, pendingApps.size(),
                !pendingApps.isEmpty() && pendingApps.get(0).isComplete());

        int missingFields = 0;
        if (pendingApps.isEmpty()) {
            if (mPaymentUiService.merchantSupportsAutofillCards()) {
                // Record all fields if basic-card is supported but no card exists.
                missingFields = AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_CARDHOLDER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_NUMBER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS;
            }
        } else if (pendingApps.get(0).isAutofillInstrument()) {
            missingFields = ((AutofillPaymentInstrument) (pendingApps.get(0))).getMissingFields();
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingPaymentFields", missingFields);
        }

        mPaymentUiService.updateAppModifiedTotals();

        SettingsAutofillAndPaymentsObserver.getInstance().registerObserver(mPaymentUiService);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        return mPaymentUiService.hasAvailableApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String getRejectShowErrorMessage() {
        return mRejectShowErrorMessage;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean disconnectForStrictShow(boolean isUserGestureShow) {
        if (!isUserGestureShow || !mSpec.getMethodData().containsKey(MethodStrings.BASIC_CARD)
                || mPaymentRequestService.getHasEnrolledInstrument()
                || mPaymentRequestService.getHasNonAutofillApp()
                || !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            return false;
        }

        mRejectShowErrorMessage = ErrorStrings.STRICT_BASIC_CARD_SHOW_REJECT;
        disconnectFromClientWithDebugMessage(
                ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet()) + " "
                        + mRejectShowErrorMessage,
                PaymentErrorReason.NOT_SUPPORTED);

        return true;
    }

    // Implements PaymentApp.InstrumentDetailsCallback:
    /** Called after retrieving payment details. */
    @Override
    public void onInstrumentDetailsReady(
            String methodName, String stringifiedDetails, PayerData payerData) {
        assert methodName != null;
        assert stringifiedDetails != null;

        if (mPaymentRequestService == null || mPaymentResponseHelper == null) {
            return;
        }

        // If the payment method was an Autofill credit card with an identifier, record its use.
        PaymentApp selectedPaymentMethod =
                (PaymentApp) mPaymentUiService.getPaymentMethodsSection().getSelectedItem();
        if (selectedPaymentMethod != null
                && selectedPaymentMethod.getPaymentAppType() == PaymentAppType.AUTOFILL
                && !selectedPaymentMethod.getIdentifier().isEmpty()) {
            PersonalDataManager.getInstance().recordAndLogCreditCardUse(
                    selectedPaymentMethod.getIdentifier());
        }

        // Showing the payment request UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mPaymentUiService.shouldSkipShowingPaymentRequestUi()
                && mPaymentUiService.getPaymentRequestUI() != null) {
            mPaymentUiService.getPaymentRequestUI().showProcessingMessageAfterUiSkip();
        }

        mJourneyLogger.setEventOccurred(Event.RECEIVED_INSTRUMENT_DETAILS);

        mPaymentResponseHelper.onPaymentDetailsReceived(methodName, stringifiedDetails, payerData);
    }

    @Override
    public void onPaymentResponseReady(PaymentResponse response) {
        if (mSkipToGPayHelper != null && !mSkipToGPayHelper.patchPaymentResponse(response)) {
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.PAYMENT_APP_INVALID_RESPONSE, PaymentErrorReason.NOT_SUPPORTED);
        }
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onPaymentResponse(response);
        mPaymentResponseHelper = null;
        if (PaymentRequestService.getObserverForTest() != null) {
            PaymentRequestService.getObserverForTest().onPaymentResponseReady();
        }
    }

    /** Called if unable to retrieve payment details. */
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
        if (mPaymentRequestService == null) return;
        mInvokedPaymentApp = null;
        if (mPaymentUiService.getMinimalUI() != null) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            mPaymentUiService.getMinimalUI().showErrorAndClose(
                    /*observer=*/this::close, R.string.payments_error_message);
            return;
        }

        // When skipping UI, any errors/cancel from fetching payment details should abort payment.
        if (mPaymentUiService.shouldSkipShowingPaymentRequestUi()) {
            assert !TextUtils.isEmpty(errorMessage);
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(errorMessage);
        } else {
            mPaymentUiService.getPaymentRequestUI().onPayButtonProcessingCancelled();
            PaymentDetailsUpdateServiceHelper.getInstance().reset();
        }
    }

    /**
     * Closes the UI and destroys native objects. If the client is still connected, then it's
     * notified of UI hiding. This ChromePaymentRequestService object can't be reused after this
     * function is called.
     */
    private void closeUIAndDestroyNativeObjects() {
        mPaymentUiService.close();
        SettingsAutofillAndPaymentsObserver.getInstance().unregisterObserver(mPaymentUiService);

        setShowingPaymentRequest(null);

        // Destroy native objects.
        mJourneyLogger.destroy();
        if (mPaymentHandlerHost != null) {
            mPaymentHandlerHost.destroy();
            mPaymentHandlerHost = null;
        }

        if (mSpec != null) {
            mSpec.destroy();
        }
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail) {
        if (mPaymentRequestService == null || !mWasRetryCalled) return;
        mPaymentRequestService.onPayerDetailChange(detail);
    }

    /**
     * @return Whether any instance of PaymentRequest has received a show() call.
     *         Don't use this function to check whether the current instance has
     *         received a show() call.
     */
    private static boolean getIsAnyPaymentRequestShowing() {
        return sShowingPaymentRequest != null;
    }

    /** @param paymentRequest The currently showing ChromePaymentRequestService. */
    private static void setShowingPaymentRequest(ChromePaymentRequestService paymentRequest) {
        assert sShowingPaymentRequest == null || paymentRequest == null;
        sShowingPaymentRequest = paymentRequest;
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onPaymentRequestUIFaviconNotAvailable() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.warnNoFavicon();
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onShippingOptionChange(String optionId) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onShippingOptionChange(optionId);
    }

    // Implement PaymentUIsObserver.onLeavingCurrentTab:
    @Override
    public void onLeavingCurrentTab(String reason) {
        if (mPaymentRequestService == null) return;
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(reason);
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onUiServiceError(String error) {
        mJourneyLogger.setAborted(AbortReason.OTHER);
        disconnectFromClientWithDebugMessage(error);
        if (PaymentRequestService.getObserverForTest() != null) {
            PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
        }
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onShippingAddressChange(PaymentAddress address) {
        if (mPaymentRequestService == null) return;
        // This updates the line items and the shipping options asynchronously.
        mPaymentRequestService.onShippingAddressChange(address);
    }
}
