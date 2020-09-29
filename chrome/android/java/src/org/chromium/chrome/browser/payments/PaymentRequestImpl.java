// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.ui.PaymentInformation;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentUIsManager;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.payments.ui.ShoppingCart;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.CanMakePaymentQuery;
import org.chromium.components.payments.CheckoutFunnelStep;
import org.chromium.components.payments.ComponentPaymentRequestImpl;
import org.chromium.components.payments.ComponentPaymentRequestImpl.Delegate;
import org.chromium.components.payments.ErrorMessageUtil;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.Event;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.NotShownReason;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryParams;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentDetailsConverter;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.components.payments.PaymentUIsObserver;
import org.chromium.components.payments.PaymentValidator;
import org.chromium.components.payments.Section;
import org.chromium.components.payments.SkipToGPayHelper;
import org.chromium.components.payments.UrlUtil;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
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
import org.chromium.payments.mojom.PaymentShippingType;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This is the Clank specific parts of {@link PaymentRequest}, with the parts shared with WebLayer
 * living in {@link ComponentPaymentRequestImpl}.
 */
public class PaymentRequestImpl
        implements BrowserPaymentRequest, PaymentRequestUI.Client, PaymentAppFactoryDelegate,
                   PaymentAppFactoryParams, PaymentRequestUpdateEventListener,
                   PaymentApp.AbortCallback, PaymentApp.InstrumentDetailsCallback,
                   PaymentResponseHelper.PaymentResponseRequesterDelegate,
                   PaymentDetailsConverter.MethodChecker, PaymentUIsManager.Delegate,
                   PaymentUIsObserver {

    private static final String TAG = "PaymentRequest";
    private static boolean sIsLocalCanMakePaymentQueryQuotaEnforcedForTest;
    /**
     * Hold the currently showing PaymentRequest. Used to prevent showing more than one
     * PaymentRequest UI per browser process.
     */
    private static PaymentRequestImpl sShowingPaymentRequest;

    // Null-check is necessary because retainers of PaymentRequestImpl could still reference
    // PaymentRequestImpl after mComponentPaymentRequestImpl is set null, e.g., crbug.com/1122148.
    @Nullable
    private ComponentPaymentRequestImpl mComponentPaymentRequestImpl;

    private final Handler mHandler = new Handler();
    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final String mTopLevelOrigin;
    private final String mPaymentRequestOrigin;
    private final Origin mPaymentRequestSecurityOrigin;
    private final String mMerchantName;
    @Nullable
    private final byte[][] mCertificateChain;
    private final JourneyLogger mJourneyLogger;
    private final boolean mIsOffTheRecord;

    private final PaymentUIsManager mPaymentUIsManager;

    @Nullable
    private final PaymentOptions mPaymentOptions;
    private final boolean mRequestShipping;
    private final boolean mRequestPayerName;
    private final boolean mRequestPayerPhone;
    private final boolean mRequestPayerEmail;
    private final int mShippingType;

    private boolean mIsCanMakePaymentResponsePending;
    private boolean mIsHasEnrolledInstrumentResponsePending;
    private boolean mIsCurrentPaymentRequestShowing;
    private boolean mWasRetryCalled;

    private boolean mHasClosed;

    private PaymentRequestSpec mSpec;
    private boolean mIsFinishedQueryingPaymentApps;
    private List<PaymentApp> mPendingApps = new ArrayList<>();
    private PaymentApp mInvokedPaymentApp;
    private boolean mHideServerAutofillCards;
    private boolean mWaitForUpdatedDetails;
    private PaymentHandlerHost mPaymentHandlerHost;

    /**
     * True when at least one url payment method identifier is specified in payment request.
     */
    private boolean mURLPaymentMethodIdentifiersSupported;

    /**
     * A mapping of the payment method names to the corresponding payment method specific data. If
     * STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT is enabled, then the key "basic-card-payment-options"
     * also maps to the following payment options:
     *  - requestPayerEmail
     *  - requestPayerName
     *  - requestPayerPhone
     *  - requestShipping
     */
    private Map<String, PaymentMethodData> mQueryForQuota = new HashMap<>();

    /**
     * There are a few situations were the Payment Request can appear, from a code perspective, to
     * be shown more than once. This boolean is used to make sure it is only logged once.
     */
    private boolean mDidRecordShowEvent;

    /** True if any of the requested payment methods are supported. */
    private boolean mCanMakePayment;

    /**
     * True after at least one usable payment app has been found and the setting allows querying
     * this value. This value can be used to respond to hasEnrolledInstrument(). Should be read only
     * after all payment apps have been queried.
     */
    private boolean mHasEnrolledInstrument;

    /**
     * Whether there's at least one app that is not an autofill card. Should be read only after all
     * payment apps have been queried.
     */
    private boolean mHasNonAutofillApp;

    /** Whether PaymentRequest.show() was invoked with a user gesture. */
    private boolean mIsUserGestureShow;

    /** The helper to create and fill the response to send to the merchant. */
    private PaymentResponseHelper mPaymentResponseHelper;

    /** If not empty, use this error message for rejecting PaymentRequest.show(). */
    private String mRejectShowErrorMessage;

    /** A helper to manage the Skip-to-GPay experimental flow. */
    private SkipToGPayHelper mSkipToGPayHelper;

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param componentPaymentRequestImpl The component side of the PaymentRequest implementation.
     * @param delegate The delegate of this class.
     */
    public PaymentRequestImpl(
            ComponentPaymentRequestImpl componentPaymentRequestImpl, Delegate delegate) {
        assert componentPaymentRequestImpl != null;
        assert delegate != null;

        mComponentPaymentRequestImpl = componentPaymentRequestImpl;
        mRenderFrameHost = componentPaymentRequestImpl.getRenderFrameHost();
        assert mRenderFrameHost != null;
        mPaymentRequestOrigin = componentPaymentRequestImpl.getPaymentRequestOrigin();
        assert mPaymentRequestOrigin != null;
        mPaymentRequestSecurityOrigin =
                componentPaymentRequestImpl.getPaymentRequestSecurityOrigin();
        assert mPaymentRequestSecurityOrigin != null;
        mTopLevelOrigin = componentPaymentRequestImpl.getTopLevelOrigin();
        assert mTopLevelOrigin != null;
        mCertificateChain = componentPaymentRequestImpl.getCertificateChain();
        mIsOffTheRecord = componentPaymentRequestImpl.isOffTheRecord();
        mDelegate = delegate;
        mWebContents = componentPaymentRequestImpl.getWebContents();
        mMerchantName = mWebContents.getTitle();
        mJourneyLogger = componentPaymentRequestImpl.getJourneyLogger();

        mPaymentOptions = componentPaymentRequestImpl.getPaymentOptions();
        assert mPaymentOptions != null;
        mRequestShipping = mPaymentOptions.requestShipping;
        mRequestPayerName = mPaymentOptions.requestPayerName;
        mRequestPayerPhone = mPaymentOptions.requestPayerPhone;
        mRequestPayerEmail = mPaymentOptions.requestPayerEmail;
        mShippingType = mPaymentOptions.shippingType;

        mComponentPaymentRequestImpl = componentPaymentRequestImpl;
        mPaymentUIsManager = new PaymentUIsManager(/*delegate=*/this,
                /*params=*/this, mWebContents, mIsOffTheRecord, mJourneyLogger, mTopLevelOrigin,
                /*observer=*/this);
        mComponentPaymentRequestImpl.registerPaymentRequestLifecycleObserver(mPaymentUIsManager);
    }

    // Implement BrowserPaymentRequest:
    @Override
    public boolean initAndValidate(PaymentMethodData[] rawMethodData, PaymentDetails details,
            boolean googlePayBridgeEligible) {
        assert mComponentPaymentRequestImpl != null;
        assert rawMethodData != null;
        assert details != null;

        boolean googlePayBridgeActivated = googlePayBridgeEligible
                && SkipToGPayHelperUtil.canActivateExperiment(mWebContents, rawMethodData);

        Map<String, PaymentMethodData> methodData =
                getValidatedMethodData(rawMethodData, googlePayBridgeActivated);

        if (methodData == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA);
            return false;
        }

        if (googlePayBridgeActivated) {
            PaymentMethodData data = methodData.get(MethodStrings.GOOGLE_PAY);
            mSkipToGPayHelper = new SkipToGPayHelper(mPaymentOptions, data.gpayBridgeData);
        }

        mQueryForQuota = new HashMap<>(methodData);
        if (mQueryForQuota.containsKey(MethodStrings.BASIC_CARD)
                && PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            PaymentMethodData paymentMethodData = new PaymentMethodData();
            paymentMethodData.stringifiedData =
                    PaymentOptionsUtils.stringifyRequestedInformation(mPaymentOptions);
            mQueryForQuota.put("basic-card-payment-options", paymentMethodData);
        }

        if (!PaymentValidator.validatePaymentDetails(details)
                || !parseAndValidateDetailsForSkipToGPayHelper(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return false;
        }

        mSpec = new PaymentRequestSpec(mPaymentOptions, details, methodData.values(),
                LocaleUtils.getDefaultLocaleString());
        if (mSpec.getRawTotal() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.TOTAL_REQUIRED);
            return false;
        }

        mPaymentUIsManager.updateDetailsOnPaymentRequestUI(
                mSpec.getPaymentDetails(), mSpec.getRawTotal(), mSpec.getRawLineItems());

        // The first time initializations and validation of all of the parameters of {@link
        // PaymentRequestParams} should be done before {@link
        // PaymentRequestLifeCycleObserver#onPaymentRequestParamsInitiated}.
        mComponentPaymentRequestImpl.getPaymentRequestLifecycleObserver()
                .onPaymentRequestParamsInitiated(
                        /*params=*/this);

        PaymentAppService.getInstance().create(/*delegate=*/this);

        // Log the various types of payment methods that were requested by the merchant.
        boolean requestedMethodGoogle = false;
        // Not to record requestedMethodBasicCard because JourneyLogger ignore the case where the
        // specified networks are unsupported. mPaymentUIsManager.merchantSupportsAutofillCards()
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
                    // mPaymentUIsManager.merchantSupportsAutofillCards() is used instead.
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
                /*requestedBasicCard=*/mPaymentUIsManager.merchantSupportsAutofillCards(),
                requestedMethodGoogle, requestedMethodOther);
        return true;
    }

    /** @return Whether the UI was built. */
    private boolean buildUI(ChromeActivity activity) {
        String error = mPaymentUIsManager.buildPaymentRequestUI(activity,
                /*isWebContentsActive=*/mDelegate.isWebContentsActive(),
                /*waitForUpdatedDetails=*/mWaitForUpdatedDetails);
        if (error != null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(error);
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceShowFailed();
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
        if (mComponentPaymentRequestImpl == null) return;

        if (mPaymentUIsManager.isShowingUI()) {
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
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceShowFailed();
            }
            return;
        }

        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.SHOW_CALLED);
        setShowingPaymentRequest(this);
        mIsCurrentPaymentRequestShowing = true;
        mIsUserGestureShow = isUserGesture;
        mWaitForUpdatedDetails = waitForUpdatedDetails;

        mJourneyLogger.setTriggerTime();
        if (disconnectIfNoPaymentMethodsSupported()) return;

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceShowFailed();
            }
            return;
        }

        if (mIsFinishedQueryingPaymentApps) {
            // Send AppListReady signal when all apps are created and request.show() is called.
            if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
                ComponentPaymentRequestImpl.getNativeObserverForTest().onAppListReady(
                        mPaymentUIsManager.getPaymentMethodsSection().getItems(),
                        mSpec.getRawTotal());
            }
            // Calculate skip ui and build ui only after all payment apps are ready and
            // request.show() is called.
            mPaymentUIsManager.calculateWhetherShouldSkipShowingPaymentRequestUi(mIsUserGestureShow,
                    mURLPaymentMethodIdentifiersSupported,
                    mComponentPaymentRequestImpl.skipUiForNonUrlPaymentMethodIdentifiers());
            if (!buildUI(chromeActivity)) return;
            if (!mPaymentUIsManager.shouldSkipShowingPaymentRequestUi()
                    && mSkipToGPayHelper == null) {
                mPaymentUIsManager.getPaymentRequestUI().show();
            }
        }

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);
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
        mPaymentUIsManager.getPaymentRequestUI().dimBackground();
    }

    private void triggerPaymentAppUiSkipIfApplicable(ChromeActivity chromeActivity) {
        // If we are skipping showing the Payment Request UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if ((mPaymentUIsManager.shouldSkipShowingPaymentRequestUi() || mSkipToGPayHelper != null)
                && mIsFinishedQueryingPaymentApps && mIsCurrentPaymentRequestShowing
                && !mWaitForUpdatedDetails) {
            assert !mPaymentUIsManager.getPaymentMethodsSection().isEmpty();
            assert mPaymentUIsManager.getPaymentRequestUI() != null;

            if (isMinimalUiApplicable()) {
                if (mPaymentUIsManager.triggerMinimalUI(chromeActivity, mSpec.getRawTotal(),
                            this::onMinimalUIReady, this::onMinimalUiConfirmed,
                            /*dismissObserver=*/this::onDismiss)) {
                    mDidRecordShowEvent = true;
                    mJourneyLogger.setEventOccurred(Event.SHOWN);
                } else {
                    disconnectFromClientWithDebugMessage(ErrorStrings.MINIMAL_UI_SUPPRESSED);
                }
                return;
            }

            assert !mPaymentUIsManager.getPaymentMethodsSection().isEmpty();
            PaymentApp selectedApp =
                    (PaymentApp) mPaymentUIsManager.getPaymentMethodsSection().getSelectedItem();
            dimBackgroundIfNotBottomSheetPaymentHandler(selectedApp);
            mDidRecordShowEvent = true;
            mJourneyLogger.setEventOccurred(Event.SKIPPED_SHOW);
            assert mSpec.getRawTotal() != null;
            // The total amount in details should be finalized at this point. So it is safe to
            // record the triggered transaction amount.
            assert !mWaitForUpdatedDetails;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
            onPayClicked(null /* selectedShippingAddress */, null /* selectedShippingOption */,
                    selectedApp);
        }
    }

    /** @return Whether the minimal UI should be shown. */
    private boolean isMinimalUiApplicable() {
        if (!mIsUserGestureShow || mPaymentUIsManager.getPaymentMethodsSection() == null
                || mPaymentUIsManager.getPaymentMethodsSection().getSize() != 1) {
            return false;
        }

        PaymentApp app =
                (PaymentApp) mPaymentUIsManager.getPaymentMethodsSection().getSelectedItem();
        if (app == null || !app.isReadyForMinimalUI() || TextUtils.isEmpty(app.accountBalance())) {
            return false;
        }

        return PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_MINIMAL_UI);
    }

    private void onMinimalUIReady() {
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onMinimalUIReady();
        }
    }

    private void onMinimalUiConfirmed(PaymentApp app) {
        mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                mSpec.getRawTotal().amount.value, false /*completed*/);
        app.disableShowingOwnUI();
        onPayClicked(null /* selectedShippingAddress */, null /* selectedShippingOption */, app);
    }

    private void onMinimalUiCompletedAndClosed() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onComplete();
        close();
    }

    /** Called after the non-minimal UI has handled {@link #complete}. */
    private void onNonMinimalUiHandledComplete() {
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onCompleteCalled();
        }
        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest().onCompleteReplied();
        }
        if (mComponentPaymentRequestImpl != null) {
            mComponentPaymentRequestImpl.onComplete();
            close();
        }
    }

    private static Map<String, PaymentMethodData> getValidatedMethodData(
            PaymentMethodData[] methodData, boolean googlePayBridgeEligible) {
        // Payment methodData are required.
        assert methodData != null;
        if (methodData.length == 0) return null;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (int i = 0; i < methodData.length; i++) {
            String method = methodData[i].supportedMethod;

            if (TextUtils.isEmpty(method)) return null;

            if (googlePayBridgeEligible) {
                // If skip-to-GPay flow is activated, ignore all other payment methods, which can be
                // either "basic-card" or "https://android.com/pay". The latter is safe to ignore
                // because merchant has already requested Google Pay.
                if (!method.equals(MethodStrings.GOOGLE_PAY)) continue;
                if (methodData[i].gpayBridgeData != null
                        && !methodData[i].gpayBridgeData.stringifiedData.isEmpty()) {
                    methodData[i].stringifiedData = methodData[i].gpayBridgeData.stringifiedData;
                }
            }
            result.put(method, methodData[i]);
        }

        return Collections.unmodifiableMap(result);
    }

    /** Called by the payment app to get updated total based on the billing address, for example. */
    @Override
    public boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails) {
        if (TextUtils.isEmpty(methodName) || stringifiedDetails == null
                || mComponentPaymentRequestImpl == null || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            return false;
        }

        mComponentPaymentRequestImpl.onPaymentMethodChange(methodName, stringifiedDetails);
        return true;
    }

    /**
     * Called by the payment app to get updated payment details based on the shipping option.
     */
    @Override
    public boolean changeShippingOptionFromInvokedApp(String shippingOptionId) {
        if (TextUtils.isEmpty(shippingOptionId) || mComponentPaymentRequestImpl == null
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

        mComponentPaymentRequestImpl.onShippingOptionChange(shippingOptionId);
        return true;
    }

    /**
     * Called by payment app to get updated payment details based on the shipping address.
     */
    @Override
    public boolean changeShippingAddressFromInvokedApp(PaymentAddress shippingAddress) {
        if (shippingAddress == null || mComponentPaymentRequestImpl == null
                || mInvokedPaymentApp == null
                || mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate() || !mRequestShipping) {
            return false;
        }

        mComponentPaymentRequestImpl.onShippingAddressChange(shippingAddress);
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
        return mPaymentUIsManager.getPaymentHandlerWebContentsForTest();
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
        return mPaymentUIsManager.clickPaymentHandlerSecurityIconForTest();
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
        return mPaymentUIsManager.clickPaymentHandlerCloseButtonForTest();
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
        return mPaymentUIsManager.confirmMinimalUIForTest();
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
        return mPaymentUIsManager.dismissMinimalUIForTest();
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

        if (mComponentPaymentRequestImpl == null) return null;

        @Nullable
        WebContents paymentHandlerWebContents = mPaymentUIsManager.showPaymentHandlerUI(
                url, mComponentPaymentRequestImpl.isOffTheRecord());
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
        if (mComponentPaymentRequestImpl == null) return;
        // mSpec.updateWith() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (mWaitForUpdatedDetails) {
            initializeWithUpdatedDetails(details);
            return;
        }

        if (mPaymentUIsManager.getPaymentRequestUI() == null) {
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
        mPaymentUIsManager.updateDetailsOnPaymentRequestUI(
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

        if (shouldShowShippingSection()
                && (mPaymentUIsManager.getUiShippingOptions().isEmpty()
                        || !TextUtils.isEmpty(details.error))
                && mPaymentUIsManager.getShippingAddressesSection().getSelectedItem() != null) {
            mPaymentUIsManager.getShippingAddressesSection().getSelectedItem().setInvalid();
            mPaymentUIsManager.getShippingAddressesSection().setSelectedItemIndex(
                    SectionInformation.INVALID_SELECTION);
            mPaymentUIsManager.getShippingAddressesSection().setErrorMessage(details.error);
        }

        boolean providedInformationToPaymentRequestUI =
                mPaymentUIsManager.enableAndUpdatePaymentRequestUIWithPaymentInfo();
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
        mPaymentUIsManager.updateDetailsOnPaymentRequestUI(
                mSpec.getPaymentDetails(), mSpec.getRawTotal(), mSpec.getRawLineItems());

        if (!TextUtils.isEmpty(details.error)) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        // Do not create shipping section When UI is not built yet. This happens when the show
        // promise gets resolved before all apps are ready.
        if (mPaymentUIsManager.getPaymentRequestUI() != null && shouldShowShippingSection()) {
            mPaymentUIsManager.createShippingSectionForPaymentRequestUI(chromeActivity);
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

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);

        if (mIsFinishedQueryingPaymentApps
                && !mPaymentUIsManager.shouldSkipShowingPaymentRequestUi()) {
            boolean providedInformationToPaymentRequestUI =
                    mPaymentUIsManager.enableAndUpdatePaymentRequestUIWithPaymentInfo();
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
        if (mComponentPaymentRequestImpl == null) return;
        // mSpec.recomputeSpecForDetails(), mSpec.selectedShippingOptionError() can be used only
        // when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (mPaymentUIsManager.getPaymentRequestUI() == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW);
            return;
        }

        mSpec.recomputeSpecForDetails();

        if (mInvokedPaymentApp != null && mInvokedPaymentApp.isWaitingForPaymentDetailsUpdate()) {
            mInvokedPaymentApp.onPaymentDetailsNotUpdated();
            return;
        }

        if (shouldShowShippingSection()
                && (mPaymentUIsManager.getUiShippingOptions().isEmpty()
                        || !TextUtils.isEmpty(mSpec.selectedShippingOptionError()))
                && mPaymentUIsManager.getShippingAddressesSection().getSelectedItem() != null) {
            mPaymentUIsManager.getShippingAddressesSection().getSelectedItem().setInvalid();
            mPaymentUIsManager.getShippingAddressesSection().setSelectedItemIndex(
                    SectionInformation.INVALID_SELECTION);
            mPaymentUIsManager.getShippingAddressesSection().setErrorMessage(
                    mSpec.selectedShippingOptionError());
        }

        boolean providedInformationToPaymentRequestUI =
                mPaymentUIsManager.enableAndUpdatePaymentRequestUIWithPaymentInfo();
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

    /**
     * Called to retrieve the data to show in the initial PaymentRequest UI.
     */
    @Override
    public void getDefaultPaymentInformation(Callback<PaymentInformation> callback) {
        mPaymentUIsManager.getDefaultPaymentInformation(callback, mIsCurrentPaymentRequestShowing,
                mIsFinishedQueryingPaymentApps, mWaitForUpdatedDetails);
    }

    // Implement PaymentUIsManager.Delegate:
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

    @Override
    public void getShoppingCart(final Callback<ShoppingCart> callback) {
        mHandler.post(callback.bind(mPaymentUIsManager.getUiShoppingCart()));
    }

    @Override
    public void getSectionInformation(@PaymentRequestUI.DataType final int optionType,
            final Callback<SectionInformation> callback) {
        mPaymentUIsManager.getSectionInformation(optionType, callback);
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionOptionSelected(@PaymentRequestUI.DataType int optionType,
            EditableOption option, Callback<PaymentInformation> callback) {
        return mPaymentUIsManager.onSectionOptionSelected(
                optionType, option, callback, mWasRetryCalled);
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionEditOption(@PaymentRequestUI.DataType int optionType, EditableOption option,
            Callback<PaymentInformation> callback) {
        return mPaymentUIsManager.onSectionEditOption(optionType, option, callback);
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionAddOption(
            @PaymentRequestUI.DataType int optionType, Callback<PaymentInformation> callback) {
        return mPaymentUIsManager.onSectionAddOption(optionType, callback);
    }

    @Override
    public boolean shouldShowShippingSection() {
        return mPaymentUIsManager.shouldShowShippingSection();
    }

    @Override
    public boolean shouldShowContactSection() {
        return mPaymentUIsManager.shouldShowContactSection();
    }

    @Override
    public void onInstrumentDetailsLoadingWithoutUI() {
        if (mComponentPaymentRequestImpl == null || mPaymentUIsManager.getPaymentRequestUI() == null
                || mPaymentResponseHelper == null) {
            return;
        }

        assert mPaymentUIsManager.getSelectedPaymentAppType() == PaymentAppType.AUTOFILL;

        mPaymentUIsManager.getPaymentRequestUI().showProcessingMessage();
    }

    @Override
    public boolean onPayClicked(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, EditableOption selectedPaymentMethod) {
        mJourneyLogger.recordCheckoutStep(CheckoutFunnelStep.PAYMENT_HANDLER_INVOKED);
        mInvokedPaymentApp = (PaymentApp) selectedPaymentMethod;

        EditableOption selectedContact = mPaymentUIsManager.getContactSection() != null
                ? mPaymentUIsManager.getContactSection().getSelectedItem()
                : null;
        mPaymentResponseHelper = new PaymentResponseHelper(selectedShippingAddress,
                selectedShippingOption, selectedContact, mInvokedPaymentApp, mPaymentOptions,
                mSkipToGPayHelper != null, this);

        // Create maps that are subsets of mMethodData and mModifiers, that contain the payment
        // methods supported by the selected payment app. If the intersection of method data
        // contains more than one payment method, the payment app is at liberty to choose (or have
        // the user choose) one of the methods.
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        Map<String, PaymentDetailsModifier> modifiers = new HashMap<>();
        boolean isGooglePaymentApp = false;
        for (String paymentMethodName : mInvokedPaymentApp.getInstrumentMethodNames()) {
            if (mSpec.getMethodData().containsKey(paymentMethodName)) {
                methodData.put(paymentMethodName, mSpec.getMethodData().get(paymentMethodName));
            }
            if (mSpec.getModifiers().containsKey(paymentMethodName)) {
                modifiers.put(paymentMethodName, mSpec.getModifiers().get(paymentMethodName));
            }
            if (paymentMethodName.equals(MethodStrings.ANDROID_PAY)
                    || paymentMethodName.equals(MethodStrings.GOOGLE_PAY)) {
                isGooglePaymentApp = true;
            }
        }

        mInvokedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        // Only native apps can use PaymentDetailsUpdateService.
        if (mInvokedPaymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance().initialize(new PackageManagerDelegate(),
                    ((AndroidPaymentApp) mInvokedPaymentApp).packageName(),
                    this /* PaymentApp.PaymentRequestUpdateEventListener */);
        }

        // Create payment options for the invoked payment app.
        PaymentOptions paymentOptions = new PaymentOptions();
        paymentOptions.requestShipping =
                mRequestShipping && mInvokedPaymentApp.handlesShippingAddress();
        paymentOptions.requestPayerName =
                mRequestPayerName && mInvokedPaymentApp.handlesPayerName();
        paymentOptions.requestPayerPhone =
                mRequestPayerPhone && mInvokedPaymentApp.handlesPayerPhone();
        paymentOptions.requestPayerEmail =
                mRequestPayerEmail && mInvokedPaymentApp.handlesPayerEmail();
        paymentOptions.shippingType =
                mRequestShipping && mInvokedPaymentApp.handlesShippingAddress()
                ? mShippingType
                : PaymentShippingType.SHIPPING;

        // Redact shipping options if the selected app cannot handle shipping.
        List<PaymentShippingOption> redactedShippingOptions =
                mInvokedPaymentApp.handlesShippingAddress()
                ? mSpec.getRawShippingOptions()
                : Collections.unmodifiableList(new ArrayList<>());
        mInvokedPaymentApp.invokePaymentApp(mSpec.getId(), mMerchantName, mTopLevelOrigin,
                mPaymentRequestOrigin, mCertificateChain, Collections.unmodifiableMap(methodData),
                mSpec.getRawTotal(), mSpec.getRawLineItems(),
                Collections.unmodifiableMap(modifiers), paymentOptions, redactedShippingOptions,
                this);

        mJourneyLogger.setEventOccurred(Event.PAY_CLICKED);
        boolean isAutofillCard = mInvokedPaymentApp.isAutofillInstrument();
        // Record what type of app was selected when "Pay" was clicked.
        if (isAutofillCard) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_CREDIT_CARD);
        } else if (isGooglePaymentApp) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_GOOGLE);
        } else {
            mJourneyLogger.setEventOccurred(Event.SELECTED_OTHER);
        }
        return !isAutofillCard;
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost = new PaymentHandlerHost(mWebContents, /*delegate=*/this);
        }
        return mPaymentHandlerHost;
    }

    @Override
    public void onDismiss() {
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(ErrorStrings.USER_CANCELLED);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    // Implement BrowserPaymentRequest:
    // This method is not supposed to be used outside this class and
    // ComponentPaymentRequestImpl.
    @Override
    public void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mComponentPaymentRequestImpl != null) {
            mComponentPaymentRequestImpl.onError(reason, debugMessage);
        }
        close();
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onConnectionTerminated();
        }
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {
        if (mComponentPaymentRequestImpl == null) return;

        if (mInvokedPaymentApp != null) {
            mInvokedPaymentApp.abortPaymentApp(/*callback=*/this);
            return;
        }
        onInstrumentAbortResult(true);
    }

    /** Called by the payment app in response to an abort request. */
    @Override
    public void onInstrumentAbortResult(boolean abortSucceeded) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onAbort(abortSucceeded);
        if (abortSucceeded) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_MERCHANT);
            close();
        } else {
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceUnableToAbort();
            }
        }
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onAbortCalled();
        }
    }

    // Implement BrowserPaymentRequest:
    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(int result) {
        if (mComponentPaymentRequestImpl == null) return;

        if (result != PaymentComplete.FAIL) {
            mJourneyLogger.setCompleted();
            if (!PaymentPreferencesUtil.isPaymentCompleteOnce()) {
                PaymentPreferencesUtil.setPaymentCompleteOnce();
            }
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, true /*completed*/);
        }

        mPaymentUIsManager.onPaymentRequestComplete(result,
                /*onMinimalUiErroredAndClosed=*/this::close, this::onMinimalUiCompletedAndClosed,
                this::onNonMinimalUiHandledComplete);
    }

    // Implement BrowserPaymentRequest:
    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mComponentPaymentRequestImpl == null) return;
        // mSpec.retry() can be used only when mSpec has not been destroyed.
        assert !mSpec.isDestroyed();

        if (!PaymentValidator.validatePaymentValidationErrors(errors)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_VALIDATION_ERRORS);
            return;
        }
        mSpec.retry(errors);

        mWasRetryCalled = true;

        mComponentPaymentRequestImpl.getPaymentRequestLifecycleObserver().onRetry(errors);
    }

    @Override
    public void onCardAndAddressSettingsClicked() {
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (context == null) {
            mJourneyLogger.setAborted(AbortReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context);
    }

    // Implement BrowserPaymentRequest:
    /** Called by the merchant website to check if the user has complete payment apps. */
    @Override
    public void canMakePayment() {
        if (mComponentPaymentRequestImpl == null) return;

        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onCanMakePaymentCalled();
        }

        if (mIsFinishedQueryingPaymentApps) {
            respondCanMakePaymentQuery();
        } else {
            mIsCanMakePaymentResponsePending = true;
        }
    }

    private void respondCanMakePaymentQuery() {
        if (mComponentPaymentRequestImpl == null) return;

        mIsCanMakePaymentResponsePending = false;

        boolean response = mCanMakePayment && mDelegate.prefsCanMakePayment();
        mComponentPaymentRequestImpl.onCanMakePayment(response
                        ? CanMakePaymentQueryResult.CAN_MAKE_PAYMENT
                        : CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);

        mJourneyLogger.setCanMakePaymentValue(
                response || mComponentPaymentRequestImpl.isOffTheRecord());

        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest()
                    .onPaymentRequestServiceCanMakePaymentQueryResponded();
        }
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onCanMakePaymentReturned();
        }
    }

    // Implement BrowserPaymentRequest:
    /** Called by the merchant website to check if the user has complete payment instruments. */
    @Override
    public void hasEnrolledInstrument() {
        if (mComponentPaymentRequestImpl == null) return;

        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest().onHasEnrolledInstrumentCalled();
        }

        if (mIsFinishedQueryingPaymentApps) {
            respondHasEnrolledInstrumentQuery(mHasEnrolledInstrument);
        } else {
            mIsHasEnrolledInstrumentResponsePending = true;
        }
    }

    private void respondHasEnrolledInstrumentQuery(boolean response) {
        if (mComponentPaymentRequestImpl == null) return;

        mIsHasEnrolledInstrumentResponsePending = false;

        if (CanMakePaymentQuery.canQuery(
                    mWebContents, mTopLevelOrigin, mPaymentRequestOrigin, mQueryForQuota)) {
            mComponentPaymentRequestImpl.onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT);
        } else if (shouldEnforceCanMakePaymentQueryQuota()) {
            mComponentPaymentRequestImpl.onHasEnrolledInstrument(
                    HasEnrolledInstrumentQueryResult.QUERY_QUOTA_EXCEEDED);
        } else {
            mComponentPaymentRequestImpl.onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.WARNING_HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.WARNING_HAS_NO_ENROLLED_INSTRUMENT);
        }

        mJourneyLogger.setHasEnrolledInstrumentValue(response || mIsOffTheRecord);

        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest()
                    .onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();
        }
        if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
            ComponentPaymentRequestImpl.getNativeObserverForTest()
                    .onHasEnrolledInstrumentReturned();
        }
    }

    /**
     * @return Whether canMakePayment() query quota should be enforced. By default, the quota is
     * enforced only on https:// scheme origins. However, the tests also enable the quota on
     * localhost and file:// scheme origins to verify its behavior.
     */
    private boolean shouldEnforceCanMakePaymentQueryQuota() {
        // If |mWebContents| is destroyed, don't bother checking the localhost or file:// scheme
        // exemption. It doesn't really matter anyways.
        return mWebContents.isDestroyed()
                || !UrlUtil.isLocalDevelopmentUrl(mWebContents.getLastCommittedUrl())
                || sIsLocalCanMakePaymentQueryQuotaEnforcedForTest;
    }

    // Implement BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        assert mComponentPaymentRequestImpl != null;
        mComponentPaymentRequestImpl.close();
        mComponentPaymentRequestImpl = null;

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
        return !mPaymentUIsManager.canUserAddCreditCard()
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
        if (mComponentPaymentRequestImpl == null) return;

        mCanMakePayment = canMakePayment;

        if (!mIsCanMakePaymentResponsePending) return;

        // canMakePayment doesn't need to wait for all apps to be queried because it only needs to
        // test the existence of a payment handler.
        respondCanMakePaymentQuery();
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onAutofillPaymentAppCreatorAvailable(AutofillPaymentAppCreator creator) {
        mPaymentUIsManager.setAutofillPaymentAppCreator(creator);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        if (mComponentPaymentRequestImpl == null) return;

        mHideServerAutofillCards |= paymentApp.isServerAutofillInstrumentReplacement();
        paymentApp.setHaveRequestedAutofillData(mPaymentUIsManager.haveRequestedAutofillData());
        mHasEnrolledInstrument |= paymentApp.canMakePayment();
        mHasNonAutofillApp |= !paymentApp.isAutofillInstrument();

        if (paymentApp.isAutofillInstrument()) {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_BASIC_CARD);
        } else if (paymentApp.getInstrumentMethodNames().contains(MethodStrings.GOOGLE_PAY)
                || paymentApp.getInstrumentMethodNames().contains(MethodStrings.ANDROID_PAY)) {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_GOOGLE);
        } else {
            mJourneyLogger.setEventOccurred(Event.AVAILABLE_METHOD_OTHER);
        }

        mPendingApps.add(paymentApp);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreationError(String errorMessage) {
        if (TextUtils.isEmpty(mRejectShowErrorMessage)) mRejectShowErrorMessage = errorMessage;
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory /* Unused */) {
        mIsFinishedQueryingPaymentApps = true;

        if (mComponentPaymentRequestImpl == null || disconnectIfNoPaymentMethodsSupported()) {
            return;
        }

        // Always return false when can make payment is disabled.
        mHasEnrolledInstrument &= mDelegate.prefsCanMakePayment();

        if (mHideServerAutofillCards) {
            List<PaymentApp> nonServerAutofillCards = new ArrayList<>();
            int numberOfPendingApps = mPendingApps.size();
            for (int i = 0; i < numberOfPendingApps; i++) {
                if (!mPendingApps.get(i).isServerAutofillInstrument()) {
                    nonServerAutofillCards.add(mPendingApps.get(i));
                }
            }
            mPendingApps = nonServerAutofillCards;
        }

        // Load the validation rules for each unique region code in the credit card billing
        // addresses and check for validity.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < mPendingApps.size(); ++i) {
            @Nullable
            String countryCode = mPendingApps.get(i).getCountryCode();
            if (countryCode != null && !uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForAddressNormalization(countryCode);
            }
        }

        mPaymentUIsManager.rankPaymentAppsForPaymentRequestUI(mPendingApps);

        // Possibly pre-select the first app on the list.
        int selection = !mPendingApps.isEmpty() && mPendingApps.get(0).canPreselect()
                ? 0
                : SectionInformation.NO_SELECTION;

        if (mIsCanMakePaymentResponsePending) {
            respondCanMakePaymentQuery();
        }

        if (mIsHasEnrolledInstrumentResponsePending) {
            respondHasEnrolledInstrumentQuery(mHasEnrolledInstrument);
        }

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceShowFailed();
            }
            return;
        }

        // The list of payment apps is ready to display.
        mPaymentUIsManager.setPaymentMethodsSection(
                new SectionInformation(PaymentRequestUI.DataType.PAYMENT_METHODS, selection,
                        new ArrayList<>(mPendingApps)));

        // Record the number suggested payment methods and whether at least one of them was
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(Section.PAYMENT_METHOD, mPendingApps.size(),
                !mPendingApps.isEmpty() && mPendingApps.get(0).isComplete());

        int missingFields = 0;
        if (mPendingApps.isEmpty()) {
            if (mPaymentUIsManager.merchantSupportsAutofillCards()) {
                // Record all fields if basic-card is supported but no card exists.
                missingFields = AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_CARDHOLDER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_NUMBER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS;
            }
        } else if (mPendingApps.get(0).isAutofillInstrument()) {
            missingFields = ((AutofillPaymentInstrument) (mPendingApps.get(0))).getMissingFields();
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingPaymentFields", missingFields);
        }

        mPendingApps.clear();

        mPaymentUIsManager.updateAppModifiedTotals();

        SettingsAutofillAndPaymentsObserver.getInstance().registerObserver(mPaymentUIsManager);

        if (mIsCurrentPaymentRequestShowing) {
            // Send AppListReady signal when all apps are created and request.show() is called.
            if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
                ComponentPaymentRequestImpl.getNativeObserverForTest().onAppListReady(
                        mPaymentUIsManager.getPaymentMethodsSection().getItems(),
                        mSpec.getRawTotal());
            }
            // Calculate skip ui and build ui only after all payment apps are ready and
            // request.show() is called, since only then whether or not should skip payment sheet UI
            // is determined.
            assert mIsFinishedQueryingPaymentApps;
            mPaymentUIsManager.calculateWhetherShouldSkipShowingPaymentRequestUi(mIsUserGestureShow,
                    mURLPaymentMethodIdentifiersSupported,
                    mComponentPaymentRequestImpl.skipUiForNonUrlPaymentMethodIdentifiers());
            if (!buildUI(chromeActivity)) return;
            if (!mPaymentUIsManager.shouldSkipShowingPaymentRequestUi()
                    && mSkipToGPayHelper == null) {
                mPaymentUIsManager.getPaymentRequestUI().show();
            }
        }

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);
    }

    /**
     * If no payment methods are supported, disconnect from the client and return true.
     * @return Whether client has been disconnected.
     */
    private boolean disconnectIfNoPaymentMethodsSupported() {
        if (!mIsFinishedQueryingPaymentApps || !mIsCurrentPaymentRequestShowing) return false;

        boolean havePaymentApps = !mPendingApps.isEmpty()
                || (mPaymentUIsManager.getPaymentMethodsSection() != null
                        && !mPaymentUIsManager.getPaymentMethodsSection().isEmpty());

        if (!mCanMakePayment
                || (!havePaymentApps && !mPaymentUIsManager.merchantSupportsAutofillCards())) {
            // All factories have responded, but none of them have apps. It's possible to add credit
            // cards, but the merchant does not support them either. The payment request must be
            // rejected.
            mJourneyLogger.setNotShown(mCanMakePayment
                            ? NotShownReason.NO_MATCHING_PAYMENT_METHOD
                            : NotShownReason.NO_SUPPORTED_PAYMENT_METHOD);
            if (mComponentPaymentRequestImpl.isOffTheRecord()) {
                // If the user is in the OffTheRecord mode, hide the absence of their payment
                // methods from the merchant site.
                disconnectFromClientWithDebugMessage(
                        ErrorStrings.USER_CANCELLED, PaymentErrorReason.USER_CANCEL);
            } else {
                if (ComponentPaymentRequestImpl.getNativeObserverForTest() != null) {
                    ComponentPaymentRequestImpl.getNativeObserverForTest().onNotSupportedError();
                }
                if (TextUtils.isEmpty(mRejectShowErrorMessage) && !isInTwa()
                        && mSpec.getMethodData().get(MethodStrings.GOOGLE_PLAY_BILLING) != null) {
                    mRejectShowErrorMessage = ErrorStrings.APP_STORE_METHOD_ONLY_SUPPORTED_IN_TWA;
                }
                disconnectFromClientWithDebugMessage(
                        ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet())
                                + (TextUtils.isEmpty(mRejectShowErrorMessage)
                                                ? ""
                                                : " " + mRejectShowErrorMessage),
                        PaymentErrorReason.NOT_SUPPORTED);
            }
            if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
                ComponentPaymentRequestImpl.getObserverForTest()
                        .onPaymentRequestServiceShowFailed();
            }
            return true;
        }

        return disconnectForStrictShow();
    }

    private boolean isInTwa() {
        return !TextUtils.isEmpty(getTwaPackageName());
    }

    /**
     * If strict show() conditions are not satisfied, disconnect from client and return true.
     * @return Whether client has been disconnected.
     */
    private boolean disconnectForStrictShow() {
        if (!mIsUserGestureShow || !mSpec.getMethodData().containsKey(MethodStrings.BASIC_CARD)
                || mHasEnrolledInstrument || mHasNonAutofillApp
                || !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            return false;
        }

        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest().onPaymentRequestServiceShowFailed();
        }
        mRejectShowErrorMessage = ErrorStrings.STRICT_BASIC_CARD_SHOW_REJECT;
        disconnectFromClientWithDebugMessage(
                ErrorMessageUtil.getNotSupportedErrorMessage(mSpec.getMethodData().keySet()) + " "
                        + mRejectShowErrorMessage,
                PaymentErrorReason.NOT_SUPPORTED);

        return true;
    }

    /** Called after retrieving payment details. */
    @Override
    public void onInstrumentDetailsReady(
            String methodName, String stringifiedDetails, PayerData payerData) {
        assert methodName != null;
        assert stringifiedDetails != null;

        if (mComponentPaymentRequestImpl == null || mPaymentResponseHelper == null) {
            return;
        }

        // If the payment method was an Autofill credit card with an identifier, record its use.
        PaymentApp selectedPaymentMethod =
                (PaymentApp) mPaymentUIsManager.getPaymentMethodsSection().getSelectedItem();
        if (selectedPaymentMethod != null
                && selectedPaymentMethod.getPaymentAppType() == PaymentAppType.AUTOFILL
                && !selectedPaymentMethod.getIdentifier().isEmpty()) {
            PersonalDataManager.getInstance().recordAndLogCreditCardUse(
                    selectedPaymentMethod.getIdentifier());
        }

        // Showing the payment request UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mPaymentUIsManager.shouldSkipShowingPaymentRequestUi()
                && mPaymentUIsManager.getPaymentRequestUI() != null) {
            mPaymentUIsManager.getPaymentRequestUI().showProcessingMessageAfterUiSkip();
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
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onPaymentResponse(response);
        mPaymentResponseHelper = null;
        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest().onPaymentResponseReady();
        }
    }

    /** Called if unable to retrieve payment details. */
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
        if (mComponentPaymentRequestImpl == null) return;
        mInvokedPaymentApp = null;
        if (mPaymentUIsManager.getMinimalUI() != null) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            mPaymentUIsManager.getMinimalUI().showErrorAndClose(
                    /*observer=*/this::close, R.string.payments_error_message);
            return;
        }

        // When skipping UI, any errors/cancel from fetching payment details should abort payment.
        if (mPaymentUIsManager.shouldSkipShowingPaymentRequestUi()) {
            assert !TextUtils.isEmpty(errorMessage);
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(errorMessage);
        } else {
            mPaymentUIsManager.getPaymentRequestUI().onPayButtonProcessingCancelled();
            PaymentDetailsUpdateServiceHelper.getInstance().reset();
        }
    }

    // Implement PaymentUIsManager.Delegate:
    @Override
    public PaymentRequestUI.Client getPaymentRequestUIClient() {
        return this;
    }

    /**
     * Closes the UI and destroys native objects. If the client is still connected, then it's
     * notified of UI hiding. This PaymentRequestImpl object can't be reused after this function is
     * called.
     */
    private void closeUIAndDestroyNativeObjects() {
        mPaymentUIsManager.ensureHideAndResetPaymentHandlerUi();
        mPaymentUIsManager.hideMinimalUI();

        if (mPaymentUIsManager.getPaymentRequestUI() != null) {
            mPaymentUIsManager.getPaymentRequestUI().close();
            ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
            if (activity != null) {
                activity.getLifecycleDispatcher().unregister(
                        mPaymentUIsManager.getPaymentRequestUI());
            }
            mPaymentUIsManager.setPaymentRequestUI(null);
            mPaymentUIsManager.getPaymentUisShowStateReconciler().onPaymentRequestUiClosed();
        }

        setShowingPaymentRequest(null);
        mIsCurrentPaymentRequestShowing = false;

        if (mPaymentUIsManager.getPaymentMethodsSection() != null) {
            for (int i = 0; i < mPaymentUIsManager.getPaymentMethodsSection().getSize(); i++) {
                EditableOption option = mPaymentUIsManager.getPaymentMethodsSection().getItem(i);
                ((PaymentApp) option).dismissInstrument();
            }
            mPaymentUIsManager.setPaymentMethodsSection(null);
        }

        mPaymentUIsManager.removeLeavingTabObservers();

        SettingsAutofillAndPaymentsObserver.getInstance().unregisterObserver(mPaymentUIsManager);

        // Destroy native objects.
        mPaymentUIsManager.destroyCurrencyFormatters();
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

    // Implement PaymentUIsManager.Delegate:
    @Override
    public void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail) {
        if (mComponentPaymentRequestImpl == null || !mWasRetryCalled) return;
        mComponentPaymentRequestImpl.onPayerDetailChange(detail);
    }

    /**
     * @return Whether any instance of PaymentRequest has received a show() call.
     *         Don't use this function to check whether the current instance has
     *         received a show() call.
     */
    private static boolean getIsAnyPaymentRequestShowing() {
        return sShowingPaymentRequest != null;
    }

    /** @param paymentRequest The currently showing PaymentRequestImpl. */
    private static void setShowingPaymentRequest(PaymentRequestImpl paymentRequest) {
        assert sShowingPaymentRequest == null || paymentRequest == null;
        sShowingPaymentRequest = paymentRequest;
    }

    @VisibleForTesting
    public static void setIsLocalCanMakePaymentQueryQuotaEnforcedForTest() {
        sIsLocalCanMakePaymentQueryQuotaEnforcedForTest = true;
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onPaymentRequestUIFaviconNotAvailable() {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.warnNoFavicon();
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onShippingOptionChange(String optionId) {
        if (mComponentPaymentRequestImpl == null) return;
        mComponentPaymentRequestImpl.onShippingOptionChange(optionId);
    }

    // Implement PaymentUIsObserver.onLeavingCurrentTab:
    @Override
    public void onLeavingCurrentTab(String reason) {
        if (mComponentPaymentRequestImpl == null) return;
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(reason);
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onUiServiceError(String error) {
        mJourneyLogger.setAborted(AbortReason.OTHER);
        disconnectFromClientWithDebugMessage(error);
        if (ComponentPaymentRequestImpl.getObserverForTest() != null) {
            ComponentPaymentRequestImpl.getObserverForTest().onPaymentRequestServiceShowFailed();
        }
    }

    // Implement PaymentUIsObserver:
    @Override
    public void onShippingAddressChange(PaymentAddress address) {
        if (mComponentPaymentRequestImpl == null) return;
        // This updates the line items and the shipping options asynchronously.
        mComponentPaymentRequestImpl.onShippingAddressChange(address);
    }
}
