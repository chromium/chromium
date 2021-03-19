// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.AndroidPaymentApp;
import org.chromium.components.payments.BrowserPaymentRequest;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
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
import org.chromium.ui.base.WindowAndroid;
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

        /**
         * Create PaymentUiService.
         * @param delegate The delegate of this instance.
         * @param webContents The WebContents of the merchant page.
         * @param isOffTheRecord Whether merchant page is in an isOffTheRecord tab.
         * @param journeyLogger The logger of the user journey.
         * @param topLevelOrigin The last committed url of webContents.
         */
        default PaymentUiService createPaymentUiService(PaymentUiService.Delegate delegate,
                PaymentRequestParams params, WebContents webContents, boolean isOffTheRecord,
                JourneyLogger journeyLogger, String topLevelOrigin) {
            return new PaymentUiService(/*delegate=*/delegate,
                    /*params=*/params, webContents, isOffTheRecord, journeyLogger, topLevelOrigin);
        }

        /**
         * Looks up the Android Activity of the given web contents. This can be null. Should never
         * be cached, because web contents can change activities, e.g., when user selects "Open in
         * Chrome" menu item.
         *
         * @param webContents The web contents for which to lookup the Android activity.
         * @return Possibly null Android activity that should never be cached.
         */
        @Nullable
        default Activity getActivity(WebContents webContents) {
            return ChromeActivity.fromWebContents(webContents);
        }

        /**
         * Creates an instance of Android payment app factory.
         * @return The instance, can be null for testing.
         */
        @Nullable
        default PaymentAppFactoryInterface createAndroidPaymentAppFactory() {
            return new AndroidPaymentAppFactory();
        }

        /**
         * Creates an instance of service-worker payment app factory.
         * @return The instance, can be null for testing.
         */
        @Nullable
        default PaymentAppFactoryInterface createServiceWorkerPaymentAppFactory() {
            return new PaymentAppServiceBridge();
        }

        /**
         * Creates an instance of Autofill payment app factory.
         * @return The instance, can be null for testing.
         */
        @Nullable
        default PaymentAppFactoryInterface createAutofillPaymentAppFactory() {
            return new AutofillPaymentAppFactory();
        }

        /**
         * Whether an autofill transaction is allowed to be made.
         * @return The instance, can be null for testing.
         */
        default boolean canMakeAutofillPayment(Map<String, PaymentMethodData> methodData) {
            return AutofillPaymentAppFactory.canMakePayments(methodData);
        }

        /**
         * @param renderFrameHost The frame that issues the payment request.
         * @return Whether the WebContents of the merchant frame is alive and visible.
         */
        default boolean isWebContentsActive(RenderFrameHost renderFrameHost) {
            return PaymentRequestServiceUtil.isWebContentsActive(renderFrameHost);
        }

        /**
         * Creates an instance of PaymentHandlerHost.
         * @param webContents The WebContents that issues the payment request.
         * @param listener The listener to payment method, shipping address, and shipping option
         *        change events
         * @return The instance.
         */
        default PaymentHandlerHost createPaymentHandlerHost(
                WebContents webContents, PaymentRequestUpdateEventListener listener) {
            return new PaymentHandlerHost(webContents, listener);
        }

        /**
         * @param webContents Any WebContents.
         * @return The TabModelSelector of the given WebContents.
         */
        @Nullable
        default TabModelSelector getTabModelSelector(WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getTabModelSelector();
        }

        /**
         * @param webContents Any WebContents.
         * @return The TabModel of the given WebContents.
         */
        @Nullable
        default TabModel getTabModel(WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getCurrentTabModel();
        }

        /**
         * @param webContents Any WebContents.
         * @return The ActivityLifecycleDispatcher of the ChromeActivity that contains the given
         *         WebContents.
         */
        @Nullable
        default ActivityLifecycleDispatcher getActivityLifecycleDispatcher(
                WebContents webContents) {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            return activity == null ? null : activity.getLifecycleDispatcher();
        }
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
        mPaymentUiService = mDelegate.createPaymentUiService(/*delegate=*/this,
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
    public boolean hasAnyCompleteApp() {
        return mPaymentUiService.hasAnyCompleteAppSuggestion();
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
            service.addUniqueFactory(mDelegate.createAndroidPaymentAppFactory(), androidFactoryId);
        }
        String swFactoryId = PaymentAppServiceBridge.class.getName();
        if (!service.containsFactory(swFactoryId)) {
            service.addUniqueFactory(mDelegate.createServiceWorkerPaymentAppFactory(), swFactoryId);
        }

        String autofillFactoryId = AutofillPaymentAppFactory.class.getName();
        if (!service.containsFactory(autofillFactoryId)) {
            service.addUniqueFactory(
                    mDelegate.createAutofillPaymentAppFactory(), autofillFactoryId);
        }
        if (mDelegate.canMakeAutofillPayment(mSpec.getMethodData())) {
            mPaymentUiService.setAutofillPaymentAppCreator(
                    AutofillPaymentAppFactory.createAppCreator(
                            /*delegate=*/delegate));
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String showOrSkipAppSelector(boolean isShowWaitingForUpdatedDetails, PaymentItem total,
            boolean shouldSkipAppSelector) {
        Activity activity = mDelegate.getActivity(mWebContents);
        if (activity == null) return ErrorStrings.ACTIVITY_NOT_FOUND;
        TabModelSelector tabModelSelector = mDelegate.getTabModelSelector(mWebContents);
        if (tabModelSelector == null) return ErrorStrings.TAB_NOT_FOUND;
        TabModel tabModel = mDelegate.getTabModel(mWebContents);
        if (tabModel == null) return ErrorStrings.TAB_NOT_FOUND;
        String error = mPaymentUiService.buildPaymentRequestUI(
                /*isWebContentsActive=*/mDelegate.isWebContentsActive(mRenderFrameHost), activity,
                tabModelSelector, tabModel);
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
            mJourneyLogger.setShown();
        }
        return null;
    }

    private void dimBackgroundIfNotPaymentHandler(PaymentApp selectedApp) {
        if (selectedApp != null
                && selectedApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP) {
            // As bottom-sheet itself has dimming effect, dimming PR is unnecessary for the
            // bottom-sheet PH. For now, service worker based payment apps are the only ones that
            // can open the bottom-sheet.
            return;
        }
        mPaymentUiService.dimBackground();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String onShowCalledAndAppsQueriedAndDetailsFinalized(boolean isUserGestureShow) {
        WindowAndroid windowAndroid = mDelegate.getWindowAndroid(mRenderFrameHost);
        if (windowAndroid == null) return ErrorStrings.WINDOW_NOT_FOUND;
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        // If we are skipping showing the app selector UI, we should call into the payment app
        // immediately after we determine the apps are ready and UI is shown.
        if (mHasSkippedAppSelector) {
            assert !mPaymentUiService.getPaymentApps().isEmpty();

            if (isMinimalUiApplicable(isUserGestureShow)) {
                if (mPaymentUiService.triggerMinimalUI(windowAndroid, mSpec.getRawTotal(),
                            this::onMinimalUIReady, this::onMinimalUiConfirmed,
                            /*dismissObserver=*/
                            ()
                                    -> onUiAborted(AbortReason.ABORTED_BY_USER,
                                            ErrorStrings.USER_CANCELLED))) {
                    mJourneyLogger.setShown();
                    return null;
                } else {
                    return ErrorStrings.MINIMAL_UI_SUPPRESSED;
                }
            }

            assert !mPaymentUiService.getPaymentApps().isEmpty();
            PaymentApp selectedApp = mPaymentUiService.getSelectedPaymentApp();
            dimBackgroundIfNotPaymentHandler(selectedApp);
            mJourneyLogger.setSkippedShow();
            invokePaymentApp(null /* selectedShippingAddress */, null /* selectedShippingOption */,
                    selectedApp);
        } else {
            mPaymentUiService.createShippingSectionIfNeeded(context);
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

        mPaymentUiService.showShippingAddressErrorIfApplicable(details.error);
        mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public String continueShowWithUpdatedDetails(
            PaymentDetails details, boolean isFinishedQueryingPaymentApps) {
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) return ErrorStrings.CONTEXT_NOT_FOUND;

        mPaymentUiService.updateDetailsOnPaymentRequestUI(details);

        if (isFinishedQueryingPaymentApps && !mHasSkippedAppSelector) {
            mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
        }
        return null;
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        mPaymentUiService.showShippingAddressErrorIfApplicable(selectedShippingOptionError);
        mPaymentUiService.enableAndUpdatePaymentRequestUIWithPaymentInfo();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean parseAndValidateDetailsFurtherIfNeeded(PaymentDetails details) {
        return mSkipToGPayHelper == null || mSkipToGPayHelper.setShippingOptionIfValid(details);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsLoading() {
        assert mPaymentUiService.getSelectedPaymentApp() == null
                || mPaymentUiService.getSelectedPaymentApp().getPaymentAppType()
                        == PaymentAppType.AUTOFILL;
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
        return selectedPaymentApp.getPaymentAppType() != PaymentAppType.AUTOFILL;
    }

    private PaymentHandlerHost getPaymentHandlerHost() {
        if (mPaymentHandlerHost == null) {
            mPaymentHandlerHost = mDelegate.createPaymentHandlerHost(
                    mWebContents, /*listener=*/mPaymentRequestService);
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
        Context context = mDelegate.getContext(mRenderFrameHost);
        if (context == null) {
            disconnectFromClientWithDebugMessage(ErrorStrings.CONTEXT_NOT_FOUND);
            return;
        }
        mPaymentUiService.onRetry(context, errors);
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
            if (firstApp.getPaymentAppType() == PaymentAppType.AUTOFILL) {
                missingFields = ((AutofillPaymentInstrument) (firstApp)).getMissingFields();
            }
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingPaymentFields", missingFields);
        }
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

    // Implement PaymentUiService.Delegate:
    @Override
    @Nullable
    public Context getContext() {
        return mDelegate.getContext(mRenderFrameHost);
    }

    // Implement PaymentUiService.Delegate:
    @Override
    @Nullable
    public ActivityLifecycleDispatcher getActivityLifecycleDispatcher() {
        return mDelegate.getActivityLifecycleDispatcher(mWebContents);
    }
}
