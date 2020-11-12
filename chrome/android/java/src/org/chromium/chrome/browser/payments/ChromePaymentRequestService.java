// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;

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
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.Event;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.NotShownReason;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentApp;
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
import org.chromium.payments.mojom.PaymentErrorReason;
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
public class ChromePaymentRequestService implements BrowserPaymentRequest,
                                                    PaymentUiService.Delegate, PaymentUIsObserver {

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
    private final PaymentOptions mPaymentOptions;
    private final boolean mRequestShipping;
    private boolean mWasRetryCalled;

    private boolean mHasClosed;

    private PaymentRequestSpec mSpec;
    private boolean mHideServerAutofillCards;
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
        String topLevelOrigin = paymentRequestService.getTopLevelOrigin();
        assert topLevelOrigin != null;
        mDelegate = delegate;
        mWebContents = paymentRequestService.getWebContents();
        mJourneyLogger = paymentRequestService.getJourneyLogger();

        mPaymentOptions = paymentRequestService.getPaymentOptions();
        assert mPaymentOptions != null;
        mRequestShipping = mPaymentOptions.requestShipping;

        mPaymentRequestService = paymentRequestService;
        mPaymentUiService = new PaymentUiService(/*delegate=*/this,
                /*params=*/mPaymentRequestService, mWebContents,
                paymentRequestService.isOffTheRecord(), mJourneyLogger, topLevelOrigin,
                /*observer=*/this);
        if (PaymentRequestService.getNativeObserverForTest() != null) {
            PaymentRequestService.getNativeObserverForTest().onPaymentUiServiceCreated(
                    mPaymentUiService);
        }
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
                    // "Other" includes https url, http url(when certificate check is bypassed) and
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

        if (!parseAndValidateDetailsFurtherIfNeeded(details)) {
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
                    AutofillPaymentAppFactory.createAppCreator(
                            /*delegate=*/mPaymentRequestService));
        }
    }

    /** @return Whether the UI was built. */
    private boolean buildUI(ChromeActivity activity, boolean isShowWaitingForUpdatedDetails) {
        String error = mPaymentUiService.buildPaymentRequestUI(activity,
                /*isWebContentsActive=*/
                PaymentRequestServiceUtil.isWebContentsActive(mRenderFrameHost),
                /*isShowWaitingForUpdatedDetails=*/isShowWaitingForUpdatedDetails);
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

    @Override
    public boolean isShowingUi() {
        return mPaymentUiService.isShowingUI();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean showAppSelector(boolean isShowWaitingForUpdatedDetails) {
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
        if (quitShowIfActivityNotFound(chromeActivity)
                || !buildUI(chromeActivity, isShowWaitingForUpdatedDetails)) {
            return false;
        }
        if (!mPaymentUiService.shouldSkipShowingPaymentRequestUi() && mSkipToGPayHelper == null) {
            mPaymentUiService.getPaymentRequestUI().show(isShowWaitingForUpdatedDetails);
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
                && !mPaymentRequestService.isShowWaitingForUpdatedDetails()) {
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
            assert !mPaymentRequestService.isShowWaitingForUpdatedDetails();
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

    // Implements BrowserPaymentRequest:
    @Override
    @Nullable
    public WebContents openPaymentHandlerWindow(GURL url) {
        if (mPaymentRequestService == null) return null;
        PaymentApp invokedPaymentApp = mPaymentRequestService.getInvokedPaymentApp();
        assert invokedPaymentApp != null;
        assert invokedPaymentApp.getPaymentAppType() == PaymentAppType.SERVICE_WORKER_APP;

        @Nullable
        WebContents paymentHandlerWebContents = mPaymentUiService.showPaymentHandlerUI(
                url, mPaymentRequestService.isOffTheRecord());
        if (paymentHandlerWebContents != null) {
            ServiceWorkerPaymentAppBridge.onOpeningPaymentAppWindow(
                    /*paymentRequestWebContents=*/mWebContents,
                    /*paymentHandlerWebContents=*/paymentHandlerWebContents);

            // UKM for payment app origin should get recorded only when the origin of the invoked
            // payment app is shown to the user.
            mJourneyLogger.setPaymentAppUkmSourceId(invokedPaymentApp.getUkmSourceId());
        }
        return paymentHandlerWebContents;
    }

    // Implement BrowserPaymentRequest:
    @Override
    public void onPaymentDetailsUpdated(
            PaymentDetails details, boolean hasNotifiedInvokedPaymentApp) {
        // This method is only called from mPaymentRequestService.
        assert mPaymentRequestService != null;

        mPaymentUiService.updateDetailsOnPaymentRequestUI(details);

        if (hasNotifiedInvokedPaymentApp) return;

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

    // Implements BrowserPaymentRequest:
    @Override
    public void continueShow() {
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        mPaymentUiService.updateDetailsOnPaymentRequestUI(mSpec.getPaymentDetails());

        // Do not create shipping section When UI is not built yet. This happens when the show
        // promise gets resolved before all apps are ready.
        if (mPaymentUiService.getPaymentRequestUI() != null
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
    @Override
    public void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError) {
        // This method is only supposed to be called by mPaymentRequestService.
        assert mPaymentRequestService != null;

        if (mPaymentUiService.shouldShowShippingSection()
                && (mPaymentUiService.getUiShippingOptions().isEmpty()
                        || !TextUtils.isEmpty(mSpec.selectedShippingOptionError()))
                && mPaymentUiService.getShippingAddressesSection().getSelectedItem() != null) {
            mPaymentUiService.getShippingAddressesSection().getSelectedItem().setInvalid();
            mPaymentUiService.getShippingAddressesSection().setSelectedItemIndex(
                    SectionInformation.INVALID_SELECTION);
            mPaymentUiService.getShippingAddressesSection().setErrorMessage(
                    selectedShippingOptionError);
        }

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
        if (!mPaymentRequestService.isShowWaitingForUpdatedDetails()) {
            assert mSpec.getRawTotal() != null;
            mJourneyLogger.recordTransactionAmount(mSpec.getRawTotal().amount.currency,
                    mSpec.getRawTotal().amount.value, false /*completed*/);
        }
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsLoading() {
        if (mPaymentUiService.getPaymentRequestUI() == null) {
            return;
        }

        assert mPaymentUiService.getSelectedPaymentAppType() == PaymentAppType.AUTOFILL;

        mPaymentUiService.getPaymentRequestUI().showProcessingMessage();
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean invokePaymentApp(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, PaymentApp selectedPaymentApp) {
        EditableOption selectedContact = mPaymentUiService.getContactSection() != null
                ? mPaymentUiService.getContactSection().getSelectedItem()
                : null;
        selectedPaymentApp.setPaymentHandlerHost(getPaymentHandlerHost());
        // Only native apps can use PaymentDetailsUpdateService.
        if (selectedPaymentApp.getPaymentAppType() == PaymentAppType.NATIVE_MOBILE_APP) {
            PaymentDetailsUpdateServiceHelper.getInstance().initialize(new PackageManagerDelegate(),
                    ((AndroidPaymentApp) selectedPaymentApp).packageName(),
                    mPaymentRequestService /* PaymentApp.PaymentRequestUpdateEventListener */);
        }
        PaymentResponseHelperInterface paymentResponseHelper = new ChromePaymentResponseHelper(
                selectedShippingAddress, selectedShippingOption, selectedContact,
                selectedPaymentApp, mPaymentOptions, mSkipToGPayHelper != null);
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
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        if (mPaymentRequestService != null) {
            mPaymentRequestService.disconnectFromClientWithDebugMessage(debugMessage, reason);
        }
        // Either closed before this method or closed by mPaymentRequestService.
        assert mHasClosed;
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
    public boolean isPaymentSheetBasedPaymentAppSupported() {
        return mPaymentUiService.canUserAddCreditCard();
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsReady() {
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
    }

    // Implements BrowserPaymentRequest:
    @Override
    public boolean patchPaymentResponseIfNeeded(PaymentResponse response) {
        return mSkipToGPayHelper == null || mSkipToGPayHelper.patchPaymentResponse(response);
    }

    // Implements BrowserPaymentRequest:
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
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
