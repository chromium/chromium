// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.Event;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.components.payments.SkipToGPayHelper;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This is the Clank specific parts of {@link PaymentRequest}, with the parts shared with WebLayer
 * living in {@link PaymentRequestService}.
 */
public class ChromePaymentRequestService
        implements BrowserPaymentRequest, PaymentUiService.Delegate {
    // Null-check is necessary because retainers of ChromePaymentRequestService could still
    // reference ChromePaymentRequestService after mPaymentRequestService is set null, e.g.,
    // crbug.com/1122148.
    @Nullable
    private PaymentRequestService mPaymentRequestService;

    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final JourneyLogger mJourneyLogger;

    private final PaymentUiService mPaymentUiService;
    private boolean mWasRetryCalled;

    private boolean mHasClosed;

    private PaymentRequestSpec mSpec;
    private boolean mHideServerAutofillCards;
    private PaymentHandlerHost mPaymentHandlerHost;

    /**
     * There are a few situations were the Payment Request can appear, from a code perspective, to
     * be shown more than once. This boolean is used to make sure it is only logged once.
     */
    private boolean mDidRecordShowEvent;

    /** A helper to manage the Skip-to-GPay experimental flow. */
    private SkipToGPayHelper mSkipToGPayHelper;
    private boolean mIsGooglePayBridgeActivated;
    /**
     * True if the browser has skipped showing the app selector UI (PaymentRequest UI).
     *
     * <p>In cases where there is a single payment app and the merchant does not request shipping
     * or billing, the browser can skip showing UI as the app selector UI is not benefiting the user
     * at all.
     */
    private boolean mHasSkippedAppSelector;

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
        mDelegate = delegate;
        mWebContents = paymentRequestService.getWebContents();
        mJourneyLogger = paymentRequestService.getJourneyLogger();
        String topLevelOrigin = paymentRequestService.getTopLevelOrigin();
        assert topLevelOrigin != null;
        mPaymentUiService = new PaymentUiService(/*delegate=*/this,
                /*params=*/paymentRequestService, mWebContents,
                paymentRequestService.isOffTheRecord(), mJourneyLogger, topLevelOrigin);
        mPaymentRequestService = paymentRequestService;
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onPaymentUiServiceCreated(
                    mPaymentUiService);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public PaymentApp getSelectedPaymentApp() {
        return mPaymentUiService.getSelectedPaymentApp();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public List<PaymentApp> getPaymentApps() {
        return mPaymentUiService.getPaymentApps();
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
        mPaymentUiService.initialize(mSpec.getPaymentDetails());
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean disconnectIfExtraValidationFails(WebContents webContents,
            Map<String, PaymentMethodData> methodData, PaymentDetails details,
            PaymentOptions options) {
        assert methodData != null;
        assert details != null;

        if (mIsGooglePayBridgeActivated) {
            PaymentMethodData data = methodData.get(MethodStrings.GOOGLE_PAY);
            mSkipToGPayHelper = new SkipToGPayHelper(options, data.gpayBridgeData);
        }

        if (!parseAndValidateDetailsFurtherIfNeeded(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return true;
        }
        return false;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void modifyQueryForQuotaCreatedIfNeeded(
            Map<String, PaymentMethodData> queryForQuota, PaymentOptions paymentOptions) {
        if (queryForQuota.containsKey(MethodStrings.BASIC_CARD)
                && PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT)) {
            PaymentMethodData paymentMethodData = new PaymentMethodData();
            paymentMethodData.stringifiedData =
                    PaymentOptionsUtils.stringifyRequestedInformation(paymentOptions);
            queryForQuota.put("basic-card-payment-options", paymentMethodData);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void addPaymentAppFactories(
            PaymentAppService service, PaymentAppFactoryDelegate delegate) {
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
                    AutofillPaymentAppFactory.createAppCreator(
                            /*delegate=*/delegate));
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String showOrSkipAppSelector(boolean isShowWaitingForUpdatedDetails, PaymentItem total,
            boolean shouldSkipAppSelector) {
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) return ErrorStrings.ACTIVITY_NOT_FOUND;
        String error = mPaymentUiService.buildPaymentRequestUI(chromeActivity,
                /*isWebContentsActive=*/
                PaymentRequestServiceUtil.isWebContentsActive(mRenderFrameHost),
                /*isShowWaitingForUpdatedDetails=*/isShowWaitingForUpdatedDetails);
        if (error != null) return error;
        // Calculate skip ui and build ui only after all payment apps are ready and
        // request.show() is called.
        boolean urlPaymentMethodIdentifiersSupported =
                PaymentRequestService.isUrlPaymentMethodIdentifiersSupported(
                        mSpec.getMethodData().keySet());
        // Only allowing payment apps that own their own UIs.
        // This excludes AutofillPaymentInstrument as its UI is rendered inline in
        // the app selector UI, thus can't be skipped.
        if (!urlPaymentMethodIdentifiersSupported && !mDelegate.skipUiForBasicCard()) {
            shouldSkipAppSelector = false;
        }
        if (mSkipToGPayHelper != null) shouldSkipAppSelector = true;

        if (shouldSkipAppSelector) {
            mHasSkippedAppSelector = true;
        } else {
            mPaymentUiService.showAppSelector(isShowWaitingForUpdatedDetails);
        }
        return null;
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
        mPaymentUiService.dimBackground();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String onShowCalledAndAppsQueriedAndDetailsFinalized(boolean isUserGestureShow) {
        // If we are skipping showing the app selector UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if (mHasSkippedAppSelector) {
            assert !mPaymentUiService.getPaymentApps().isEmpty();
            assert mPaymentUiService.isPaymentRequestUiAlive();

            if (isMinimalUiApplicable(isUserGestureShow)) {
                ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
                if (chromeActivity == null) return ErrorStrings.ACTIVITY_NOT_FOUND;
                if (mPaymentUiService.triggerMinimalUI(chromeActivity, mSpec.getRawTotal(),
                            this::onMinimalUIReady, this::onMinimalUiConfirmed,
                            /*dismissObserver=*/
                            ()
                                    -> onUiAborted(AbortReason.ABORTED_BY_USER,
                                            ErrorStrings.USER_CANCELLED))) {
                    mDidRecordShowEvent = true;
                    mJourneyLogger.setEventOccurred(Event.SHOWN);
                    return null;
                } else {
                    return ErrorStrings.MINIMAL_UI_SUPPRESSED;
                }
            }

            assert !mPaymentUiService.getPaymentApps().isEmpty();
            PaymentApp selectedApp = mPaymentUiService.getSelectedPaymentApp();
            dimBackgroundIfNotBottomSheetPaymentHandler(selectedApp);
            mDidRecordShowEvent = true;
            mJourneyLogger.setEventOccurred(Event.SKIPPED_SHOW);
            assert mSpec.getRawTotal() != null;
            // The total amount in details should be finalized at this point. So it is safe to
            // record the triggered transaction amount.
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
            invokePaymentApp(null /* selectedShippingAddress */, null /* selectedShippingOption */,
                    selectedApp);
        }
        return null;
    }

    /**
     * @param isUserGestureShow Whether PaymentRequest.show() was invoked with a user gesture.
     * @return Whether the minimal UI should be shown.
     */
    private boolean isMinimalUiApplicable(boolean isUserGestureShow) {
        if (!isUserGestureShow || mPaymentUiService.getPaymentApps().size() != 1) {
            return false;
        }

        PaymentApp app = mPaymentUiService.getSelectedPaymentApp();
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

    // Implements BrowserPaymentRequest:
    @Override
    public void modifyMethodDataIfNeeded(@Nullable Map<String, PaymentMethodData> methodDataMap) {
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

    // Implements BrowserPaymentRequest:
    @Override
    @Nullable
    public WebContents openPaymentHandlerWindow(
            GURL url, boolean isOffTheRecord, long ukmSourceId) {
        @Nullable
        WebContents paymentHandlerWebContents =
                mPaymentUiService.showPaymentHandlerUI(url, isOffTheRecord);
        if (paymentHandlerWebContents != null) {
            ServiceWorkerPaymentAppBridge.onOpeningPaymentAppWindow(
                    /*paymentRequestWebContents=*/mWebContents,
                    /*paymentHandlerWebContents=*/paymentHandlerWebContents);

            // UKM for payment app origin should get recorded only when the origin of the invoked
            // payment app is shown to the user.
            mJourneyLogger.setPaymentAppUkmSourceId(ukmSourceId);
        }
        return paymentHandlerWebContents;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
        mPaymentUiService.updateDetailsOnPaymentRequestUI(details);

        if (hasNotifiedInvokedPaymentApp) return;

        String detailsError = mSpec.getPaymentDetails().error;
        mPaymentUiService.showShippingAddressErrorIfApplicable(detailsError);

        boolean providedInformationToPaymentRequestUI =
                mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String continueShow(boolean isFinishedQueryingPaymentApps) {
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) return ErrorStrings.ACTIVITY_NOT_FOUND;

        mPaymentUiService.updateDetailsOnPaymentRequestUI(mSpec.getPaymentDetails());

        // Do not create shipping section When UI is not built yet. This happens when the show
        // promise gets resolved before all apps are ready.
        if (mPaymentUiService.isPaymentRequestUiAlive()
                && mPaymentUiService.shouldShowShippingSection()) {
            mPaymentUiService.createShippingSectionForPaymentRequestUI(chromeActivity);
        }

        // Triggered transaction amount gets recorded when both of the following conditions are met:
        // 1- Either Event.Shown or Event.SKIPPED_SHOW bits are set showing that transaction is
        // triggered (mDidRecordShowEvent == true). 2- The total amount in details won't change
        // (mPaymentRequestService.isShowWaitingForUpdatedDetails() == false). Record the
        // transaction amount only when the triggered condition is already met. Otherwise it will
        // get recorded when triggered condition becomes true.
        if (mDidRecordShowEvent) {
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, /*completed=*/false);
        }

        if (isFinishedQueryingPaymentApps && !mHasSkippedAppSelector) {
            boolean providedInformationToPaymentRequestUI =
                    mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
            if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        mPaymentUiService.showShippingAddressErrorIfApplicable(selectedShippingOptionError);

        boolean providedInformationToPaymentRequestUI =
                mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        if (providedInformationToPaymentRequestUI) recordShowEventAndTransactionAmount();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean parseAndValidateDetailsFurtherIfNeeded(PaymentDetails details) {
        return mSkipToGPayHelper == null || mSkipToGPayHelper.setShippingOptionIfValid(details);
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void recordShowEventAndTransactionAmount() {
        if (mDidRecordShowEvent) return;
        mDidRecordShowEvent = true;
        mJourneyLogger.setEventOccurred(Event.SHOWN);
        // Record the triggered transaction amount only when the total amount in details is
        // finalized (i.e. mPaymentRequestService.isShowWaitingForUpdatedDetails() == false).
        // Otherwise it will get recorded when the updated details become available.
        if (mPaymentRequestService != null
                && !mPaymentRequestService.isShowWaitingForUpdatedDetails()) {
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsLoading() {
        assert mPaymentUiService.getSelectedPaymentAppType() == PaymentAppType.AUTOFILL;
        mPaymentUiService.showProcessingMessage();
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean invokePaymentApp(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, PaymentApp selectedPaymentApp) {
        if (mPaymentRequestService == null || mSpec == null || mSpec.isDestroyed()) return false;
        selectedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        // Only native apps can use PaymentDetailsUpdateService.
        if (selectedPaymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance().initialize(new PackageManagerDelegate(),
                    ((AndroidPaymentApp) selectedPaymentApp).packageName(),
                    mPaymentRequestService /* PaymentApp.PaymentRequestUpdateEventListener */);
        }
        PaymentResponseHelperInterface paymentResponseHelper =
                new ChromePaymentResponseHelper(selectedShippingAddress, selectedShippingOption,
                        mPaymentUiService.getSelectedContact(), selectedPaymentApp,
                        mSpec.getPaymentOptions(), mSkipToGPayHelper != null);
        mPaymentRequestService.invokePaymentApp(selectedPaymentApp, paymentResponseHelper);
        return !selectedPaymentApp.isAutofillInstrument();
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost =
                    new PaymentHandlerHost(mWebContents, /*delegate=*/mPaymentRequestService);
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
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(
                    debugMessage, PaymentErrorReason.USER_CANCEL);
        }
        close();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void complete(int result, Runnable onCompleteHandled) {
        if (result != PaymentComplete.FAIL && !PaymentPreferencesUtil.isPaymentCompleteOnce()) {
            PaymentPreferencesUtil.setPaymentCompleteOnce();
        }

        mPaymentUiService.onPaymentRequestComplete(result,
                /*onMinimalUiErroredAndClosed=*/this::close, onCompleteHandled);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onRetry(PaymentValidationErrors errors) {
        mWasRetryCalled = true;
        mPaymentUiService.onRetry(errors);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mPaymentRequestService != null) {
            mPaymentRequestService.close();
            mPaymentRequestService = null;
        }

        mPaymentUiService.close();
        SettingsAutofillAndPaymentsObserver.getInstance().unregisterObserver(mPaymentUiService);

        if (mPaymentHandlerHost != null) {
            mPaymentHandlerHost.destroy();
            mPaymentHandlerHost = null;
        }
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        mHideServerAutofillCards |= paymentApp.isServerAutofillInstrumentReplacement();
        paymentApp.setHaveRequestedAutofillData(mPaymentUiService.haveRequestedAutofillData());
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

        mPaymentUiService.setPaymentApps(pendingApps);

        int missingFields = 0;
        if (mPaymentUiService.getPaymentApps().isEmpty()) {
            if (mPaymentUiService.merchantSupportsAutofillCards()) {
                // Record all fields if basic-card is supported but no card exists.
                missingFields = AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_CARDHOLDER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_NUMBER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS;
            }
        } else {
            PaymentApp firstApp = mPaymentUiService.getPaymentApps().get(0);
            if (firstApp.isAutofillInstrument()) {
                missingFields = ((AutofillPaymentInstrument) (firstApp)).getMissingFields();
            }
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingPaymentFields", missingFields);
        }

        SettingsAutofillAndPaymentsObserver.getInstance().registerObserver(mPaymentUiService);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean hasAvailableApps() {
        return mPaymentUiService.hasAvailableApps();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean isPaymentSheetBasedPaymentAppSupported() {
        return mPaymentUiService.canUserAddCreditCard();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsReady() {
        // If the payment app was an Autofill credit card with an identifier, record its use.
        PaymentApp selectedPaymentApp = mPaymentUiService.getSelectedPaymentApp();
        if (selectedPaymentApp != null
                && selectedPaymentApp.getPaymentAppType() == PaymentAppType.AUTOFILL
                && !selectedPaymentApp.getIdentifier().isEmpty()) {
            PersonalDataManager.getInstance().recordAndLogCreditCardUse(
                    selectedPaymentApp.getIdentifier());
        }

        // Showing the app selector UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mHasSkippedAppSelector) {
            mPaymentUiService.showProcessingMessageAfterUiSkip();
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean patchPaymentResponseIfNeeded(PaymentResponse response) {
        return mSkipToGPayHelper == null || mSkipToGPayHelper.patchPaymentResponse(response);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
        if (mPaymentUiService.isShowingMinimalUi()) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            mPaymentUiService.closeMinimalUiOnError(this::close);
            return;
        }

        // When skipping UI, any errors/cancel from fetching payment details should abort payment.
        if (mHasSkippedAppSelector) {
            assert !TextUtils.isEmpty(errorMessage);
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(errorMessage);
        } else {
            mPaymentUiService.onPayButtonProcessingCancelled();
            PaymentDetailsUpdateServiceHelper.getInstance().reset();
        }
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail) {
        if (mPaymentRequestService == null || !mWasRetryCalled) return;
        mPaymentRequestService.onPayerDetailChange(detail);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onPaymentRequestUIFaviconNotAvailable() {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.warnNoFavicon();
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onShippingOptionChange(String optionId) {
        if (mPaymentRequestService == null) return;
        mPaymentRequestService.onShippingOptionChange(optionId);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onLeavingCurrentTab(String reason) {
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(reason);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onUiServiceError(String error) {
        mJourneyLogger.setAborted(AbortReason.OTHER);
        disconnectFromClientWithDebugMessage(error);
        if (PaymentRequestService.getObserverForTest() != null) {
            PaymentRequestService.getObserverForTest().onPaymentRequestServiceShowFailed();
        }
    }

    // Implement PaymentUiService.Delegate:
    @Override
    public void onShippingAddressChange(PaymentAddress address) {
        if (mPaymentRequestService == null) return;
        // This updates the line items and the shipping options asynchronously.
        mPaymentRequestService.onShippingAddressChange(address);
    }
}
