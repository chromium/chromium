// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AddressNormalizerFactory;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ChromePaymentRequestService;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.PaymentPreferencesUtil;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.payments.ShippingStrings;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.FocusChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.autofill.AddressNormalizer.NormalizedAddressRequestDelegate;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.Completable;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentUiServiceTestInterface;
import org.chromium.components.payments.Section;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;
import java.util.Set;

/**
 * This class manages all of the UIs related to payment. The UI logic of {@link
 * ChromePaymentRequestService} should be moved into this class.
 */
public class PaymentUiService
        implements SettingsAutofillAndPaymentsObserver.Observer,
                PaymentHandlerUiObserver,
                FocusChangedObserver,
                PaymentUiServiceTestInterface,
                NormalizedAddressRequestDelegate,
                PaymentRequestUI.Client,
                LayoutStateObserver {

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PaymentRequestAddressEditorMode.ADD_NEW_ADDRESS,
        PaymentRequestAddressEditorMode.EDIT_EXISTING_ADDRESS,
        PaymentRequestAddressEditorMode.COUNT,
    })
    private @interface PaymentRequestAddressEditorMode {
        int ADD_NEW_ADDRESS = 0;
        int EDIT_EXISTING_ADDRESS = 1;
        int COUNT = 2;
    }

    /** Limit in the number of suggested items in a section. */
    /* package */ static final int SUGGESTIONS_LIMIT = 4;

    // Reverse order of the comparator to sort in descending order of completeness scores.
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            (a, b) -> PaymentAppComparator.compareCompletablesByCompleteness(b, a);
    private final Comparator<PaymentApp> mPaymentAppComparator;

    private final boolean mIsOffTheRecord;
    private final Handler mHandler = new Handler();
    private final Queue<Runnable> mRetryQueue = new LinkedList<>();
    private final TabModelSelectorObserver mSelectorObserver;
    private final TabModelObserver mTabModelObserver;
    private ContactEditor mContactEditor;
    private PaymentHandlerCoordinator mPaymentHandlerUi;
    private Callback<PaymentInformation> mPaymentInformationCallback;
    private SectionInformation mUiShippingOptions;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final String mTopLevelOriginFormattedForDisplay;
    private final String mMerchantName;
    private final Map<String, CurrencyFormatter> mCurrencyFormatterMap;
    private final AddressEditor mAddressEditor;
    private final PaymentUisShowStateReconciler mPaymentUisShowStateReconciler;
    private final PaymentRequestParams mParams;
    private final JourneyLogger mJourneyLogger;

    private PaymentRequestUI mPaymentRequestUI;
    private ShoppingCart mUiShoppingCart;
    private boolean mHasInitialized;
    private boolean mHasClosed;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private boolean mHaveRequestedAutofillData = true;
    private List<AutofillProfile> mAutofillProfiles;
    private TabModelSelector mObservedTabModelSelector;
    private TabModel mObservedTabModel;
    private LayoutStateProvider mLayoutStateProvider;

    /** The delegate of this class. */
    public interface Delegate {
        /** Dispatch the payer detail change event if needed. */
        void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail);

        /**
         * @return Whether {@link ChromePaymentRequestService#onRetry} has been called.
         */
        boolean wasRetryCalled();

        // TODO(crbug.com/40728675): The return semantics is not intuitive for this method; the
        // method should not take the selectedShippingAddress, selectedShippingOption parameters of
        // UI and autofill semantics.
        /**
         * Invokes the selected payment app.
         *
         * @param selectedShippingAddress the shipping address selected from the payment request UI.
         * @param selectedShippingOption The shipping option selected from the payment request UI.
         * @param selectedPaymentApp The selected payment app.
         * @return Whether the spinner should be displayed. Autofill cards show a CVC prompt, so the
         *     spinner is hidden in that case. Other payment apps typically show a spinner.
         */
        boolean invokePaymentApp(
                EditableOption selectedShippingAddress,
                EditableOption selectedShippingOption,
                PaymentApp selectedPaymentApp);

        /**
         * Invoked when the UI service has been aborted.
         * @param reason The reason for the aborting, as defined by {@link AbortReason}.
         * @param debugMessage The debug message for the aborting.
         */
        void onUiAborted(@AbortReason int reason, String debugMessage);

        /** Called when favicon not available for payment request UI. */
        void onPaymentRequestUIFaviconNotAvailable();

        /**
         * Called when the user is leaving the current tab (e.g., tab switched or tab overview mode
         * is shown), upon which the PaymentRequest service should be closed.
         * @param reason The reason of leaving the current tab, to be used as debug message for the
         *         developers.
         */
        void onLeavingCurrentTab(String reason);

        /**
         * Called when the user's selected shipping option has changed.
         * @param optionId The option id of the selected shipping option.
         */
        void onShippingOptionChange(String optionId);

        /**
         * Called when the shipping address has changed by the user.
         * @param address The changed shipping address.
         */
        void onShippingAddressChange(org.chromium.payments.mojom.PaymentAddress address);

        /**
         * Called when the Payment UI service quits with an error. The observer should stop
         * referencing the Payment UI service.
         * @param error The diagnostic message that's exposed to developers.
         */
        void onUiServiceError(String error);

        /**
         * @return The context of the current activity, can be null when WebContents has been
         *         destroyed, the activity is gone, the window is closed, etc.
         */
        @Nullable
        Context getContext();

        /** @return The ActivityLifecycleDispatcher of the current ChromeActivity. */
        @Nullable
        ActivityLifecycleDispatcher getActivityLifecycleDispatcher();
    }

    /**
     * This class is to coordinate the show state of a bottom sheet UI (i.e., expandable payment
     * handler) and the Payment Request UI so that these visibility rules are enforced:
     * 1. At most one UI is shown at any moment in case the Payment Request UI obstructs the bottom
     * sheet.
     * 2. Bottom sheet is prioritized to show over Payment Request UI
     */
    public class PaymentUisShowStateReconciler {
        // Whether the bottom sheet is showing.
        private boolean mShowingBottomSheet;
        // Whether to show the Payment Request UI when the bottom sheet is not being shown.
        private boolean mShouldShowDialog;

        /**
         * Show the Payment Request UI dialog when the bottom sheet is hidden, i.e., if the bottom
         * sheet hidden, show the dialog immediately; otherwise, show the dialog after the bottom
         * sheet hides.
         */
        /* package */ void showPaymentRequestDialogWhenNoBottomSheet() {
            mShouldShowDialog = true;
            updatePaymentRequestDialogShowState();
        }

        /** Hide the Payment Request UI dialog. */
        /* package */ void hidePaymentRequestDialog() {
            mShouldShowDialog = false;
            updatePaymentRequestDialogShowState();
        }

        /** A callback invoked when the Payment Request UI is closed. */
        private void onPaymentRequestUiClosed() {
            assert mPaymentRequestUI == null;
            mShouldShowDialog = false;
        }

        /** A callback invoked when the bottom sheet is shown, to enforce the visibility rules. */
        public void onBottomSheetShown() {
            mShowingBottomSheet = true;
            updatePaymentRequestDialogShowState();
        }

        /** A callback invoked when the bottom sheet is hidden, to enforce the visibility rules. */
        /* package */ void onBottomSheetClosed() {
            mShowingBottomSheet = false;
            updatePaymentRequestDialogShowState();
        }

        private void updatePaymentRequestDialogShowState() {
            if (mPaymentRequestUI == null) return;
            boolean isSuccess =
                    mPaymentRequestUI.setVisible(!mShowingBottomSheet && mShouldShowDialog);
            if (!isSuccess) {
                mDelegate.onUiServiceError(ErrorStrings.FAIL_TO_SHOW_PAYMENT_REQUEST_UI);
            }
        }
    }

    /**
     * Create PaymentUiService.
     * @param delegate The delegate of this instance.
     * @param webContents The WebContents of the merchant page.
     * @param isOffTheRecord Whether merchant page is in an isOffTheRecord tab.
     * @param journeyLogger The logger of the user journey.
     * @param topLevelOrigin The last committed url of webContents.
     */
    public PaymentUiService(
            Delegate delegate,
            PaymentRequestParams params,
            WebContents webContents,
            boolean isOffTheRecord,
            JourneyLogger journeyLogger,
            String topLevelOrigin) {
        assert !params.hasClosed();
        mDelegate = delegate;
        mParams = params;

        // Do not persist changes on disk in OffTheRecord mode.
        mAddressEditor =
                new AddressEditor(
                        PersonalDataManagerFactory.getForProfile(
                                Profile.fromWebContents(webContents)),
                        /* saveToDisk= */ !isOffTheRecord);
        mJourneyLogger = journeyLogger;
        mWebContents = webContents;
        mTopLevelOriginFormattedForDisplay = topLevelOrigin;
        mMerchantName = webContents.getTitle();

        mPaymentUisShowStateReconciler = new PaymentUisShowStateReconciler();
        mCurrencyFormatterMap = new HashMap<>();
        mIsOffTheRecord = isOffTheRecord;
        mPaymentAppComparator = new PaymentAppComparator(/* params= */ mParams);
        mSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        mDelegate.onLeavingCurrentTab(ErrorStrings.TAB_SWITCH);
                    }
                };
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (tab == null || tab.getId() != lastId) {
                            mDelegate.onLeavingCurrentTab(ErrorStrings.TAB_SWITCH);
                        }
                    }
                };
    }

    // Implements LayoutStateObserver:
    @Override
    public void onStartedShowing(int layoutType) {
        mDelegate.onLeavingCurrentTab(ErrorStrings.TAB_OVERVIEW_MODE);
    }

    /**
     * Creates the shipping section for the app selector UI if needed. This method should be called
     * when UI has been built and payment details has been finalized.
     * @param context The activity context.
     */
    public void createShippingSectionIfNeeded(Context context) {
        if (!shouldShowShippingSection()) return;
        createShippingSectionForPaymentRequestUI(context);
    }

    /**
     * @return Whether the PaymentRequest UI is alive. The UI comes to live when
     *         buildPaymentRequestUi() has been called to create it; it stops being alive when
     *         close() is called to destroy it.
     */
    public boolean isPaymentRequestUiAlive() {
        return mPaymentRequestUI != null;
    }

    /** @return The payment apps. */
    public List<PaymentApp> getPaymentApps() {
        List<PaymentApp> paymentApps = new ArrayList<>();
        if (mPaymentMethodsSection == null) return paymentApps;
        for (EditableOption each : mPaymentMethodsSection.getItems()) {
            paymentApps.add((PaymentApp) each);
        }
        return paymentApps;
    }

    /**
     * Whether the payment apps includes at least one that is "complete" which is defined
     * by {@link PaymentApp#isComplete()}. This method can be called only after
     * {@link #setPaymentApps}.
     * @return The result.
     */
    public boolean hasAnyCompleteAppSuggestion() {
        List<PaymentApp> apps = getPaymentApps();
        return !apps.isEmpty() && apps.get(0).isComplete();
    }

    /**
     * Returns the selected payment app, if any.
     * @return The selected payment app or null if none selected.
     */
    public @Nullable PaymentApp getSelectedPaymentApp() {
        return mPaymentMethodsSection == null
                ? null
                : (PaymentApp) mPaymentMethodsSection.getSelectedItem();
    }

    /**
     * Loads the payment apps into the app selector UI (aka, PaymentRequest UI).
     * @param apps The payment apps to be loaded into the app selector UI.
     */
    public void setPaymentApps(List<PaymentApp> apps) {
        Collections.sort(apps, mPaymentAppComparator);
        // Possibly pre-select the first app on the list.
        int selection =
                !apps.isEmpty() && apps.get(0).canPreselect() ? 0 : SectionInformation.NO_SELECTION;

        // The list of payment apps is ready to display.
        mPaymentMethodsSection =
                new SectionInformation(
                        PaymentRequestUI.DataType.PAYMENT_METHODS,
                        selection,
                        new ArrayList<>(apps));

        updateAppModifiedTotals();

        SettingsAutofillAndPaymentsObserver.getInstance().registerObserver(this);
    }

    /**
     * The UI model of the shopping cart, including the total. Each item includes a label and a
     * price string. This data is passed to the UI.
     */
    // Implements PaymentRequestUI.Delegate:
    @Override
    public void getShoppingCart(Callback<ShoppingCart> callback) {
        mHandler.post(callback.bind(mUiShoppingCart));
    }

    /** @return The selected contact, can be null. */
    public @Nullable AutofillContact getSelectedContact() {
        return mContactSection != null ? (AutofillContact) mContactSection.getSelectedItem() : null;
    }

    /** Get the contact editor on PaymentRequest UI. */
    private ContactEditor getContactEditor() {
        return mContactEditor;
    }

    /** @return The autofill profiles. */
    private List<AutofillProfile> getAutofillProfiles() {
        return mAutofillProfiles;
    }

    /** @return Whether PaymentRequestUI has requested autofill data. */
    public boolean haveRequestedAutofillData() {
        return mHaveRequestedAutofillData;
    }

    /**
     * Called when the merchant calls complete() to complete the payment request.
     * @param result The completion status of the payment request, defined in {@link
     *         PaymentComplete}, provided by the merchant with
     * PaymentResponse.complete(paymentResult).
     * @param onUiCompleted The function called when the opened UI has handled the completion.
     */
    public void onPaymentRequestComplete(int result, Runnable onUiCompleted) {
        // Update records of the used payment app for sorting payment apps next time.
        PaymentApp paymentApp = getSelectedPaymentApp();
        assert paymentApp != null;
        String selectedPaymentMethod = paymentApp.getIdentifier();
        PaymentPreferencesUtil.increasePaymentAppUseCount(selectedPaymentMethod);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(
                selectedPaymentMethod, System.currentTimeMillis());

        // TODO(crbug.com/40173498): The caller should execute the function at onUiCompleted
        // directly instead of passing the Runnable here, because there are no asynchronous
        // operations in this code path.
        onUiCompleted.run();
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onAddressUpdated(AutofillAddress address) {
        address.setShippingAddressLabelWithCountry();

        if (mShippingAddressesSection != null) {
            mShippingAddressesSection.addAndSelectOrUpdateItem(address);
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
        }

        if (mContactSection != null) {
            mContactSection.addOrUpdateWithAutofillAddress(address);
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);
        }
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onAddressDeleted(String guid) {
        // TODO: Delete the address from getShippingAddressesSection() and
        // getContactSection(). Note that we only displayed
        // SUGGESTIONS_LIMIT addresses, so we may want to add back previously ignored addresses.
    }

    /**
     * Initializes the payment UI service.
     * @param details The PaymentDetails provided by the merchant.
     */
    public void initialize(PaymentDetails details) {
        assert !mParams.hasClosed();
        updateDetailsOnPaymentRequestUI(details);

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(Profile.fromWebContents(mWebContents));
        if (PaymentOptionsUtils.requestAnyInformation(mParams.getPaymentOptions())) {
            mAutofillProfiles =
                    Collections.unmodifiableList(
                            personalDataManager.getProfilesToSuggest(
                                    /* includeNameInLabel= */ false));
        }

        if (mParams.getPaymentOptions().requestShipping) {
            boolean haveCompleteShippingAddress = false;
            for (int i = 0; i < mAutofillProfiles.size(); i++) {
                if (AutofillAddress.checkAddressCompletionStatus(
                                mAutofillProfiles.get(i), personalDataManager)
                        == AutofillAddress.CompletionStatus.COMPLETE) {
                    haveCompleteShippingAddress = true;
                    break;
                }
            }
            mHaveRequestedAutofillData &= haveCompleteShippingAddress;
        }

        PaymentOptions options = mParams.getPaymentOptions();
        if (PaymentOptionsUtils.requestAnyContactInformation(mParams.getPaymentOptions())) {
            // Do not persist changes on disk in OffTheRecord mode.
            mContactEditor =
                    new ContactEditor(
                            options.requestPayerName,
                            options.requestPayerPhone,
                            options.requestPayerEmail,
                            /* saveToDisk= */ !mIsOffTheRecord,
                            personalDataManager);
            boolean haveCompleteContactInfo = false;
            for (int i = 0; i < getAutofillProfiles().size(); i++) {
                AutofillProfile profile = getAutofillProfiles().get(i);
                if (getContactEditor()
                                .checkContactCompletionStatus(
                                        profile.getFullName(),
                                        profile.getPhoneNumber(),
                                        profile.getEmailAddress())
                        == ContactEditor.COMPLETE) {
                    haveCompleteContactInfo = true;
                    break;
                }
            }
            mHaveRequestedAutofillData &= haveCompleteContactInfo;
        }
        mHasInitialized = true;
    }

    /**
     * Called after {@link PaymentRequest#retry} is invoked.
     * @param context The context of the main activity.
     * @param errors The payment validation errors.
     */
    public void onRetry(Context context, PaymentValidationErrors errors) {
        // Remove all payment apps except the selected one.
        assert mPaymentMethodsSection != null;
        PaymentApp selectedApp = getSelectedPaymentApp();
        assert selectedApp != null;
        mPaymentMethodsSection =
                new SectionInformation(
                        PaymentRequestUI.DataType.PAYMENT_METHODS,
                        /* selection= */ 0,
                        new ArrayList<>(Arrays.asList(selectedApp)));
        mPaymentRequestUI.updateSection(
                PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);

        // Go back to the payment sheet
        mPaymentRequestUI.onPayButtonProcessingCancelled();
        if (!TextUtils.isEmpty(errors.error)) {
            mPaymentRequestUI.setRetryErrorMessage(errors.error);
        } else {
            mPaymentRequestUI.setRetryErrorMessage(
                    context.getResources().getString(R.string.payments_error_message));
        }

        if (shouldShowShippingSection() && hasShippingAddressError(errors.shippingAddress)) {
            mRetryQueue.add(
                    () -> {
                        mAddressEditor.setAddressErrors(errors.shippingAddress);
                        AutofillAddress selectedAddress =
                                (AutofillAddress) mShippingAddressesSection.getSelectedItem();
                        editAddress(selectedAddress);
                    });
        }

        if (shouldShowContactSection() && hasPayerError(errors.payer)) {
            mRetryQueue.add(
                    () -> {
                        mContactEditor.setPayerErrors(errors.payer);
                        AutofillContact selectedContact =
                                (AutofillContact) mContactSection.getSelectedItem();
                        editContactOnPaymentRequestUI(selectedContact);
                    });
        }

        if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
    }

    private boolean hasShippingAddressError(AddressErrors errors) {
        return !TextUtils.isEmpty(errors.addressLine)
                || !TextUtils.isEmpty(errors.city)
                || !TextUtils.isEmpty(errors.country)
                || !TextUtils.isEmpty(errors.dependentLocality)
                || !TextUtils.isEmpty(errors.organization)
                || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.postalCode)
                || !TextUtils.isEmpty(errors.recipient)
                || !TextUtils.isEmpty(errors.region)
                || !TextUtils.isEmpty(errors.sortingCode);
    }

    private boolean hasPayerError(PayerErrors errors) {
        return !TextUtils.isEmpty(errors.name)
                || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.email);
    }

    /** @return The selected payment app type. */
    private @PaymentAppType int getSelectedPaymentAppType() {
        PaymentApp paymentApp = getSelectedPaymentApp();
        return paymentApp == null ? PaymentAppType.UNDEFINED : paymentApp.getPaymentAppType();
    }

    /** Sets the modifier for the order summary based on the given app, if any. */
    private void updateOrderSummary(@Nullable PaymentApp app) {
        if (mParams.hasClosed()) return;
        PaymentDetailsModifier modifier = getModifier(app);
        PaymentItem total = modifier == null ? null : modifier.total;
        if (total == null) total = mParams.getRawTotal();

        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(total.amount);
        mUiShoppingCart.setTotal(
                new LineItem(
                        total.label,
                        formatter.getFormattedCurrencyCode(),
                        formatter.format(total.amount.value),
                        /* isPending= */ false));
        mUiShoppingCart.setAdditionalContents(
                modifier == null
                        ? null
                        : getLineItems(Arrays.asList(modifier.additionalDisplayItems)));
        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateOrderSummarySection(mUiShoppingCart);
        }
    }

    /** @return The first modifier that matches the given app, or null. */
    private @Nullable PaymentDetailsModifier getModifier(@Nullable PaymentApp app) {
        if (mParams.hasClosed()) return null;
        Map<String, PaymentDetailsModifier> modifiers = mParams.getUnmodifiableModifiers();
        if (modifiers.isEmpty() || app == null) return null;
        // Makes a copy to ensure it is modifiable.
        Set<String> methodNames = new HashSet<>(app.getInstrumentMethodNames());
        methodNames.retainAll(modifiers.keySet());
        if (methodNames.isEmpty()) return null;

        for (String methodName : methodNames) {
            PaymentDetailsModifier modifier = modifiers.get(methodName);
            if (app.isValidForPaymentMethodData(methodName, modifier.methodData)) {
                return modifier;
            }
        }

        return null;
    }

    /** Updates the modifiers for payment apps and order summary. */
    private void updateAppModifiedTotals() {
        if (mParams.hasClosed() || mParams.getMethodData().isEmpty()) return;
        if (mPaymentMethodsSection == null) return;

        for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
            PaymentApp app = (PaymentApp) mPaymentMethodsSection.getItem(i);
            PaymentDetailsModifier modifier = getModifier(app);
            app.setModifiedTotal(
                    modifier == null || modifier.total == null
                            ? null
                            : getOrCreateCurrencyFormatter(modifier.total.amount)
                                    .format(modifier.total.amount.value));
        }

        updateOrderSummary(getSelectedPaymentApp());
    }

    /**
     * Gets currency formatter for a given PaymentCurrencyAmount,
     * creates one if no existing instance is found.
     *
     * @param amount The given payment amount.
     */
    private CurrencyFormatter getOrCreateCurrencyFormatter(PaymentCurrencyAmount amount) {
        String key = amount.currency;
        CurrencyFormatter formatter = mCurrencyFormatterMap.get(key);
        if (formatter == null) {
            formatter = new CurrencyFormatter(amount.currency, Locale.getDefault());
            mCurrencyFormatterMap.put(key, formatter);
        }
        return formatter;
    }

    /**
     * Converts a list of payment items and returns their parsed representation.
     *
     * @param items The payment items to parse. Can be null.
     * @return A list of valid line items.
     */
    private List<LineItem> getLineItems(@Nullable List<PaymentItem> items) {
        // Line items are optional.
        if (items == null) return new ArrayList<>();

        List<LineItem> result = new ArrayList<>(items.size());
        for (int i = 0; i < items.size(); i++) {
            PaymentItem item = items.get(i);
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(item.amount);
            result.add(
                    new LineItem(
                            item.label,
                            isMixedOrChangedCurrency() ? formatter.getFormattedCurrencyCode() : "",
                            formatter.format(item.amount.value),
                            item.pending));
        }

        return Collections.unmodifiableList(result);
    }

    private boolean isMixedOrChangedCurrency() {
        return mCurrencyFormatterMap.size() > 1;
    }

    /**
     * Load required currency formatter for a given PaymentDetails.
     *
     * Note that the cache (mCurrencyFormatterMap) is not cleared for
     * updated payment details so as to indicate the currency has been changed.
     *
     * @param details The given payment details.
     */
    private void loadCurrencyFormattersForPaymentDetails(PaymentDetails details) {
        if (details.total != null) {
            getOrCreateCurrencyFormatter(details.total.amount);
        }

        if (details.displayItems != null) {
            for (PaymentItem item : details.displayItems) {
                getOrCreateCurrencyFormatter(item.amount);
            }
        }

        if (details.shippingOptions != null) {
            for (PaymentShippingOption option : details.shippingOptions) {
                getOrCreateCurrencyFormatter(option.amount);
            }
        }

        if (details.modifiers != null) {
            for (PaymentDetailsModifier modifier : details.modifiers) {
                if (modifier.total != null) getOrCreateCurrencyFormatter(modifier.total.amount);
                for (PaymentItem displayItem : modifier.additionalDisplayItems) {
                    getOrCreateCurrencyFormatter(displayItem.amount);
                }
            }
        }
    }

    /**
     * Converts a list of shipping options and returns their parsed representation.
     *
     * @param options The raw shipping options to parse. Can be null.
     * @return The UI representation of the shipping options.
     */
    private SectionInformation getShippingOptions(@Nullable PaymentShippingOption[] options) {
        // Shipping options are optional.
        if (options == null || options.length == 0) {
            return new SectionInformation(PaymentRequestUI.DataType.SHIPPING_OPTIONS);
        }

        List<EditableOption> result = new ArrayList<>();
        int selectedItemIndex = SectionInformation.NO_SELECTION;
        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(option.amount);
            String currencyPrefix =
                    isMixedOrChangedCurrency()
                            ? formatter.getFormattedCurrencyCode() + "\u0020"
                            : "";
            result.add(
                    new EditableOption(
                            option.id,
                            option.label,
                            currencyPrefix + formatter.format(option.amount.value),
                            null));
            if (option.selected) selectedItemIndex = i;
        }

        return new SectionInformation(
                PaymentRequestUI.DataType.SHIPPING_OPTIONS,
                selectedItemIndex,
                Collections.unmodifiableList(result));
    }

    /** Destroy the currency formatters. */
    private void destroyCurrencyFormatters() {
        for (CurrencyFormatter formatter : mCurrencyFormatterMap.values()) {
            assert formatter != null;
            // Ensures the native implementation of currency formatter does not leak.
            formatter.destroy();
        }
        mCurrencyFormatterMap.clear();
    }

    /** Notifies the UI about the changes in selected payment method. */
    private void onSelectedPaymentMethodUpdated() {
        mPaymentRequestUI.selectedPaymentMethodUpdated(
                new PaymentInformation(
                        mUiShoppingCart,
                        mShippingAddressesSection,
                        mUiShippingOptions,
                        mContactSection,
                        mPaymentMethodsSection));
    }

    /**
     * Update Payment Request UI with the update event's information and enable the UI. This method
     * should be called when the user interface is disabled with a "↻" spinner being displayed. The
     * user is unable to interact with the user interface until this method is called.
     */
    public void enableAndUpdatePaymentRequestUIWithPaymentInfo() {
        if (mPaymentInformationCallback != null && mPaymentMethodsSection != null) {
            providePaymentInformationToPaymentRequestUI();
        } else {
            mPaymentRequestUI.updateOrderSummarySection(mUiShoppingCart);
            if (shouldShowShippingSection()) {
                mPaymentRequestUI.updateSection(
                        PaymentRequestUI.DataType.SHIPPING_OPTIONS, mUiShippingOptions);
            }
        }
    }

    /**
     * Shows the shipping address error if any.
     * @param error The shipping address error, can be null.
     */
    public void showShippingAddressErrorIfApplicable(@Nullable String error) {
        if (shouldShowShippingSection()
                && (mUiShippingOptions.isEmpty() || !TextUtils.isEmpty(error))
                && mShippingAddressesSection.getSelectedItem() != null) {
            mShippingAddressesSection.getSelectedItem().setInvalid();
            mShippingAddressesSection.setSelectedItemIndex(SectionInformation.INVALID_SELECTION);
            mShippingAddressesSection.setErrorMessage(error);
        }
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public boolean shouldShowShippingSection() {
        if (mParams.hasClosed() || !mParams.getPaymentOptions().requestShipping) return false;

        PaymentApp selectedApp = getSelectedPaymentApp();
        return selectedApp == null || !selectedApp.handlesShippingAddress();
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public boolean shouldShowContactSection() {
        PaymentApp selectedApp = getSelectedPaymentApp();
        if (mParams.hasClosed()) return false;
        PaymentOptions options = mParams.getPaymentOptions();
        if (options.requestPayerName && (selectedApp == null || !selectedApp.handlesPayerName())) {
            return true;
        }
        if (options.requestPayerPhone
                && (selectedApp == null || !selectedApp.handlesPayerPhone())) {
            return true;
        }
        if (options.requestPayerEmail
                && (selectedApp == null || !selectedApp.handlesPayerEmail())) {
            return true;
        }

        return false;
    }

    // Implement PaymentHandlerUiObserver:
    @Override
    public void onPaymentHandlerUiClosed() {
        mPaymentUisShowStateReconciler.onBottomSheetClosed();
        mPaymentHandlerUi = null;
    }

    // Implement PaymentHandlerUiObserver:
    @Override
    public void onPaymentHandlerUiShown() {
        assert mPaymentHandlerUi != null;
        mPaymentUisShowStateReconciler.onBottomSheetShown();
    }

    /**
     * Create and show the (BottomSheet) PaymentHandler UI.
     *
     * @param url The URL of the payment app.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *     successful; null if failed.
     */
    public @Nullable WebContents showPaymentHandlerUI(GURL url) {
        if (mPaymentHandlerUi != null) return null;
        PaymentHandlerCoordinator paymentHandlerUi = new PaymentHandlerCoordinator();
        WebContents paymentHandlerWebContents =
                paymentHandlerUi.show(
                        /* paymentRequestWebContents= */ mWebContents, url, /* uiObserver= */ this);
        if (paymentHandlerWebContents == null) {
            paymentHandlerUi.hide();
            return null;
        }
        mPaymentHandlerUi = paymentHandlerUi;

        return paymentHandlerWebContents;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    public WebContents getPaymentHandlerWebContentsForTest() {
        if (mPaymentHandlerUi == null) return null;
        return mPaymentHandlerUi.getWebContentsForTest();
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    public boolean clickPaymentHandlerSecurityIconForTest() {
        if (mPaymentHandlerUi == null) return false;
        mPaymentHandlerUi.clickSecurityIconForTest();
        return true;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    public boolean clickPaymentHandlerCloseButtonForTest() {
        if (mPaymentHandlerUi == null) return false;
        mPaymentHandlerUi.clickCloseButtonForTest();
        return true;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    public boolean closeDialogForTest() {
        if (!mHasClosed) close();
        return true;
    }

    /** Provide PaymentInformation to the PaymentRequest UI. */
    public void providePaymentInformationToPaymentRequestUI() {
        // Do not display service worker payment apps summary in single line so as to display its
        // origin completely.
        mPaymentMethodsSection.setDisplaySelectedItemSummaryInSingleLineInNormalMode(
                getSelectedPaymentAppType() != PaymentAppType.SERVICE_WORKER_APP);
        mPaymentInformationCallback.onResult(
                new PaymentInformation(
                        mUiShoppingCart,
                        mShippingAddressesSection,
                        mUiShippingOptions,
                        mContactSection,
                        mPaymentMethodsSection));
        mPaymentInformationCallback = null;
    }

    /**
     * Edit the contact information on the PaymentRequest UI.
     * @param toEdit The information to edit, allowed to be null.
     **/
    private void editContactOnPaymentRequestUI(@Nullable final AutofillContact toEdit) {
        mContactEditor.edit(
                toEdit,
                new Callback<AutofillContact>() {
                    @Override
                    public void onResult(AutofillContact editedContact) {
                        if (mPaymentRequestUI == null) return;

                        if (editedContact != null) {
                            mContactEditor.setPayerErrors(null);

                            // A partial or complete contact came back from the editor (could have
                            // been from adding/editing or cancelling out of the edit flow).
                            if (!editedContact.isComplete()) {
                                // If the contact is not complete according to the requirements of
                                // the flow, unselect it (editor can return incomplete information
                                // when cancelled).
                                mContactSection.setSelectedItemIndex(
                                        SectionInformation.NO_SELECTION);
                            } else if (toEdit == null) {
                                // Contact is complete and we were in the "Add flow": add an item to
                                // the list.
                                mContactSection.addAndSelectItem(editedContact);
                            } else {
                                mDelegate.dispatchPayerDetailChangeEventIfNeeded(
                                        editedContact.toPayerDetail());
                            }
                            // If contact is complete and (toEdit != null), no action needed: the
                            // contact was already selected in the UI.
                        }
                        // If |editedContact| is null, the user has cancelled out of the "Add flow".
                        // No action to take (if a contact was selected in the UI, it will stay
                        // selected).

                        mPaymentRequestUI.updateSection(
                                PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);

                        if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
                    }
                });
    }

    /**
     * Edit the address on the PaymentRequest UI.
     * @param toEdit The address to be updated with, allowed to be null.
     */
    private void editAddress(@Nullable final AutofillAddress toEdit) {
        mAddressEditor.edit(
                toEdit,
                new Callback<AutofillAddress>() {
                    @Override
                    public void onResult(AutofillAddress editedAddress) {
                        if (mPaymentRequestUI == null) return;

                        if (editedAddress != null) {
                            mAddressEditor.setAddressErrors(null);

                            // Sets or updates the shipping address label.
                            editedAddress.setShippingAddressLabelWithCountry();

                            // A partial or complete address came back from the editor (could have
                            // been from adding/editing or cancelling out of the edit flow).
                            if (!editedAddress.isComplete()) {
                                // If the address is not complete, unselect it (editor can return
                                // incomplete information when cancelled).
                                mShippingAddressesSection.setSelectedItemIndex(
                                        SectionInformation.NO_SELECTION);
                                providePaymentInformationToPaymentRequestUI();
                            } else {
                                if (toEdit == null) {
                                    // Address is complete and user was in the "Add flow": add an
                                    // item to the list.
                                    mShippingAddressesSection.addAndSelectItem(editedAddress);
                                }

                                if (mContactSection != null) {
                                    // Update |mContactSection| with the new/edited
                                    // address, which will update an existing item or add a new one
                                    // to the end of the list.
                                    mContactSection.addOrUpdateWithAutofillAddress(editedAddress);
                                    mPaymentRequestUI.updateSection(
                                            PaymentRequestUI.DataType.CONTACT_DETAILS,
                                            mContactSection);
                                }

                                startShippingAddressChangeNormalization(editedAddress);
                            }
                        } else {
                            providePaymentInformationToPaymentRequestUI();
                        }

                        if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
                    }
                });

        @PaymentRequestAddressEditorMode
        int addressEditorMode =
                toEdit == null
                        ? PaymentRequestAddressEditorMode.ADD_NEW_ADDRESS
                        : PaymentRequestAddressEditorMode.EDIT_EXISTING_ADDRESS;
        RecordHistogram.recordEnumeratedHistogram(
                "PaymentRequest.AddressEditorTrigerred",
                addressEditorMode,
                PaymentRequestAddressEditorMode.COUNT);
    }

    /**
     * Dims the background of the payment UIs. Precondition: isPaymentRequestUiAlive() needs to be
     * true for the method to take effect.
     */
    public void dimBackground() {
        if (mPaymentRequestUI == null) return;
        mPaymentRequestUI.dimBackground();
    }

    /**
     * Shows the app selector UI. Precondition: isPaymentRequestUiAlive() needs to be true for
     * the method to take effect.
     * @param isShowWaitingForUpdatedDetails
     *        Whether showing payment app or the app selector is blocked on the updated payment
     *        details.
     */
    public void showAppSelector(boolean isShowWaitingForUpdatedDetails) {
        if (mPaymentRequestUI == null) return;
        mPaymentRequestUI.show(isShowWaitingForUpdatedDetails);
    }

    /**
     *  Shows the processing message after payment details have been loaded in the case the
     *  app selector UI has been skipped. Precondition: isPaymentRequestUiAlive() needs to be
     *  true for the method to take effect.
     */
    public void showProcessingMessageAfterUiSkip() {
        if (mPaymentRequestUI != null) mPaymentRequestUI.showProcessingMessageAfterUiSkip();
    }

    /**
     * Called when user cancelled out of the UI that was shown after they clicked [PAY] button.
     * Precondition: isPaymentRequestUiAlive() needs to be true for the method to take effect.
     */
    public void onPayButtonProcessingCancelled() {
        if (mPaymentRequestUI != null) mPaymentRequestUI.onPayButtonProcessingCancelled();
    }

    /**
     * Build the PaymentRequest UI.
     * @param isWebContentsActive Whether the merchant's WebContents is active.
     * @param activity The activity of the current tab.
     * @param tabModelSelector The tab model selector of the current tab.
     * @param tabModel The tab model of the current tab.
     * @return The error message if built unsuccessfully; null otherwise.
     */
    public @Nullable String buildPaymentRequestUI(
            boolean isWebContentsActive,
            Activity activity,
            TabModelSelector tabModelSelector,
            TabModel tabModel) {
        // Payment methods section must be ready before building the rest of the UI. This is because
        // shipping and contact sections (when requested by merchant) are populated depending on
        // whether or not the selected payment app (if such exists) can provide the required
        // information.
        assert mPaymentMethodsSection != null;

        assert activity != null;
        assert tabModelSelector != null;
        assert tabModel != null;

        // Only the currently selected tab is allowed to show the payment UI.
        if (!isWebContentsActive) return ErrorStrings.CANNOT_SHOW_IN_BACKGROUND_TAB;

        if (mParams.hasClosed()) return ErrorStrings.PAYMENT_REQUEST_IS_ABORTING;

        // Catch any time the user switches tabs. Because the dialog is modal, a user shouldn't be
        // allowed to switch tabs, which can happen if the user receives an external Intent.
        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
        }
        mObservedTabModelSelector = tabModelSelector;
        mObservedTabModelSelector.addObserver(mSelectorObserver);
        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
        }
        mObservedTabModel = tabModel;
        mObservedTabModel.addObserver(mTabModelObserver);

        // Catch any time the user enters the overview mode and dismiss the payment UI.
        LayoutStateProvider layoutStateProvider =
                LayoutManagerProvider.from(mWebContents.getTopLevelNativeWindow());
        if (layoutStateProvider != null) {
            if (mLayoutStateProvider != null) {
                mLayoutStateProvider.removeObserver(this);
            }
            if (layoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                return ErrorStrings.TAB_OVERVIEW_MODE;
            }
            mLayoutStateProvider = layoutStateProvider;
            mLayoutStateProvider.addObserver(this);
        }

        if (shouldShowContactSection()) {
            mContactSection =
                    new ContactDetailsSection(
                            activity, mAutofillProfiles, mContactEditor, mJourneyLogger);
        }

        mPaymentRequestUI =
                new PaymentRequestUI(
                        activity,
                        /* client= */ this,
                        !PaymentPreferencesUtil.isPaymentCompleteOnce(),
                        mMerchantName,
                        mTopLevelOriginFormattedForDisplay,
                        SecurityStateModel.getSecurityLevelForWebContents(mWebContents),
                        new ShippingStrings(mParams.getPaymentOptions().shippingType),
                        mPaymentUisShowStateReconciler,
                        Profile.fromWebContents(mWebContents));
        ActivityLifecycleDispatcher dispatcher = mDelegate.getActivityLifecycleDispatcher();
        if (dispatcher != null) {
            dispatcher.register(mPaymentRequestUI); // registered as a PauseResumeWithNativeObserver
        }

        final FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(
                Profile.fromWebContents(mWebContents),
                mWebContents.getLastCommittedUrl(),
                activity.getResources().getDimensionPixelSize(R.dimen.payments_favicon_size),
                (bitmap, iconUrl) -> {
                    if (bitmap == null) {
                        mDelegate.onPaymentRequestUIFaviconNotAvailable();
                    }
                    if (mPaymentRequestUI != null && bitmap != null) {
                        mPaymentRequestUI.setTitleBitmap(bitmap);
                    }
                    faviconHelper.destroy();
                });

        // Add the callback to change the label of shipping addresses depending on the focus.
        if (mParams.getPaymentOptions().requestShipping) {
            setShippingAddressSectionFocusChangedObserverForPaymentRequestUI();
        }

        mAddressEditor.setEditorDialog(mPaymentRequestUI.getEditorDialog());
        if (mContactEditor != null) {
            mContactEditor.setEditorDialog(mPaymentRequestUI.getEditorDialog());
        }
        return null;
    }

    /** Create a shipping section for PaymentRequest UI. */
    private void createShippingSectionForPaymentRequestUI(Context context) {
        List<AutofillAddress> addresses = new ArrayList<>();
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(Profile.fromWebContents(mWebContents));
        for (int i = 0; i < mAutofillProfiles.size(); i++) {
            AutofillProfile profile = mAutofillProfiles.get(i);
            mAddressEditor.addPhoneNumberIfValid(profile.getPhoneNumber());

            // Only suggest addresses that have a street address.
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                addresses.add(new AutofillAddress(context, profile, personalDataManager));
            }
        }

        // Suggest complete addresses first.
        Collections.sort(addresses, COMPLETENESS_COMPARATOR);

        // Limit the number of suggestions.
        addresses = addresses.subList(0, Math.min(addresses.size(), SUGGESTIONS_LIMIT));

        // Load the validation rules for each unique region code.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < addresses.size(); ++i) {
            String countryCode =
                    AutofillAddress.getCountryCode(
                            addresses.get(i).getProfile(), personalDataManager);
            if (!uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                AddressNormalizerFactory.getInstance()
                        .loadRulesForAddressNormalization(countryCode);
            }
        }

        // Automatically select the first address if one is complete and if the merchant does
        // not require a shipping address to calculate shipping costs.
        boolean hasCompleteShippingAddress = !addresses.isEmpty() && addresses.get(0).isComplete();
        int firstCompleteAddressIndex = SectionInformation.NO_SELECTION;
        if (mUiShippingOptions.getSelectedItem() != null && hasCompleteShippingAddress) {
            firstCompleteAddressIndex = 0;

            // The initial label for the selected shipping address should not include the
            // country.
            addresses.get(firstCompleteAddressIndex).setShippingAddressLabelWithoutCountry();
        }

        // Log the number of suggested shipping addresses and whether at least one of them is
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(
                Section.SHIPPING_ADDRESS, addresses.size(), hasCompleteShippingAddress);

        mShippingAddressesSection =
                new SectionInformation(
                        PaymentRequestUI.DataType.SHIPPING_ADDRESSES,
                        firstCompleteAddressIndex,
                        addresses);
    }

    // Implements PaymentRequestUi.Delegate:
    @Override
    public void getSectionInformation(
            @PaymentRequestUI.DataType final int optionType,
            final Callback<SectionInformation> callback) {
        SectionInformation result = null;
        switch (optionType) {
            case PaymentRequestUI.DataType.SHIPPING_ADDRESSES:
                result = mShippingAddressesSection;
                break;
            case PaymentRequestUI.DataType.SHIPPING_OPTIONS:
                result = mUiShippingOptions;
                break;
            case PaymentRequestUI.DataType.CONTACT_DETAILS:
                result = mContactSection;
                break;
            case PaymentRequestUI.DataType.PAYMENT_METHODS:
                result = mPaymentMethodsSection;
                break;
            default:
                assert false;
        }
        mHandler.post(callback.bind(result));
    }

    // Implement PaymentRequestSection.FocusChangedObserver:
    @Override
    public void onFocusChanged(@PaymentRequestUI.DataType int dataType, boolean willFocus) {
        assert dataType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES;

        if (mShippingAddressesSection.getSelectedItem() == null) return;

        AutofillAddress selectedAddress =
                (AutofillAddress) mShippingAddressesSection.getSelectedItem();

        // The label should only include the country if the view is focused.
        if (willFocus) {
            selectedAddress.setShippingAddressLabelWithCountry();
        } else {
            selectedAddress.setShippingAddressLabelWithoutCountry();
        }

        mPaymentRequestUI.updateSection(
                PaymentRequestUI.DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public boolean onPayClicked(
            EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption,
            EditableOption selectedPaymentMethod) {
        return mDelegate.invokePaymentApp(
                selectedShippingAddress,
                selectedShippingOption,
                (PaymentApp) selectedPaymentMethod);
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public int onSectionAddOption(
            @PaymentRequestUI.DataType int optionType, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            editAddress(null);
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            editContactOnPaymentRequestUI(null);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        } else if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            // TODO(crbug.com/40182225): Either remove DataType.PAYMENT_METHODS entirely, or
            // just remove this branch.
            assert false : "Cannot edit PAYMENT_METHODS";
            return PaymentRequestUI.SelectionResult.NONE;
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionEditOption(
            @PaymentRequestUI.DataType int optionType,
            EditableOption option,
            Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            editAddress((AutofillAddress) option);
            mPaymentInformationCallback = callback;

            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        }

        if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            editContactOnPaymentRequestUI((AutofillContact) option);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            // TODO(crbug.com/40182225): Either remove DataType.PAYMENT_METHODS entirely, or
            // just remove this branch.
            assert false : "Cannot edit PAYMENT_METHODS";
            return PaymentRequestUI.SelectionResult.NONE;
        }

        assert false;
        return PaymentRequestUI.SelectionResult.NONE;
    }

    /** Set a change observer for the shipping address section on the PaymentRequest UI. */
    private void setShippingAddressSectionFocusChangedObserverForPaymentRequestUI() {
        mPaymentRequestUI.setShippingAddressSectionFocusChangedObserver(this);
    }

    /**
     * Update the details related fields on the PaymentRequest UI.
     * @param details The details whose information is used for the update.
     */
    public void updateDetailsOnPaymentRequestUI(PaymentDetails details) {
        loadCurrencyFormattersForPaymentDetails(details);
        // Total is never pending.
        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(details.total.amount);
        LineItem uiTotal =
                new LineItem(
                        details.total.label,
                        formatter.getFormattedCurrencyCode(),
                        formatter.format(details.total.amount.value),
                        /* isPending= */ false);

        List<PaymentItem> itemList =
                details.displayItems == null
                        ? new ArrayList<>()
                        : Arrays.asList(details.displayItems);
        List<LineItem> uiLineItems = getLineItems(itemList);

        mUiShoppingCart = new ShoppingCart(uiTotal, uiLineItems);

        if (mUiShippingOptions == null || details.shippingOptions != null) {
            mUiShippingOptions = getShippingOptions(details.shippingOptions);
        }

        updateAppModifiedTotals();
    }

    /** Removes all of the observers that observe users leaving the tab. */
    private void removeLeavingTabObservers() {
        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
            mObservedTabModelSelector = null;
        }

        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
            mObservedTabModel = null;
        }

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(this);
            mLayoutStateProvider = null;
        }
    }

    // Implements PaymentRequestUI.Client:
    @Override
    public void getDefaultPaymentInformation(
            boolean isShowWaitingForUpdatedDetails, Callback<PaymentInformation> callback) {
        mPaymentInformationCallback = callback;

        if (isShowWaitingForUpdatedDetails) return;

        mHandler.post(
                () -> {
                    if (mPaymentRequestUI != null) providePaymentInformationToPaymentRequestUI();
                });
    }

    /**
     * The implementation of {@link PaymentRequestUI.Client#onSectionOptionSelected}.
     * @param optionType Data being updated.
     * @param option Value of the data being updated.
     * @param callback The callback after an asynchronous check has completed.
     * @return The result of the selection.
     */
    // Implements PaymentRequestUI.Delegate:
    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionOptionSelected(
            @PaymentRequestUI.DataType int optionType,
            EditableOption option,
            Callback<PaymentInformation> callback) {
        Context context = mDelegate.getContext();
        if (context == null) return PaymentRequestUI.SelectionResult.NONE;

        boolean wasRetryCalled = mDelegate.wasRetryCalled();
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            AutofillAddress address = (AutofillAddress) option;
            if (address.isComplete()) {
                mShippingAddressesSection.setSelectedItem(option);
                startShippingAddressChangeNormalization(address);
            } else {
                editAddress(address);
            }
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.SHIPPING_OPTIONS) {
            // This may update the line items.
            mUiShippingOptions.setSelectedItem(option);
            mDelegate.onShippingOptionChange(option.getIdentifier());
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            AutofillContact contact = (AutofillContact) option;
            if (contact.isComplete()) {
                mContactSection.setSelectedItem(option);
                if (!wasRetryCalled) return PaymentRequestUI.SelectionResult.NONE;
                mDelegate.dispatchPayerDetailChangeEventIfNeeded(contact.toPayerDetail());
            } else {
                editContactOnPaymentRequestUI(contact);
                if (!wasRetryCalled) return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
            }
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            if (shouldShowShippingSection() && mShippingAddressesSection == null) {
                createShippingSectionForPaymentRequestUI(context);
            }
            if (shouldShowContactSection() && mContactSection == null) {
                mContactSection =
                        new ContactDetailsSection(
                                context, mAutofillProfiles, mContactEditor, mJourneyLogger);
            }
            onSelectedPaymentMethodUpdated();
            PaymentApp paymentApp = (PaymentApp) option;

            updateOrderSummary(paymentApp);
            mPaymentMethodsSection.setSelectedItem(option);
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public void onDismiss() {
        mDelegate.onUiAborted(AbortReason.ABORTED_BY_USER, ErrorStrings.USER_CANCELLED);
    }

    // Implements PaymentRequestUI.Delegate:
    @Override
    public void onCardAndAddressSettingsClicked() {
        Context context = mDelegate.getContext();
        if (context == null) {
            mDelegate.onUiAborted(AbortReason.OTHER, ErrorStrings.CONTEXT_NOT_FOUND);
            return;
        }

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context);
    }

    // Implements PersonalDataManager.NormalizedAddressRequestDelegate:
    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        Context context = mDelegate.getContext();

        // Can happen if the tab is closed during the normalization process.
        if (context == null) {
            mDelegate.onUiServiceError(ErrorStrings.CONTEXT_NOT_FOUND);
            return;
        }

        // Don't reuse the selected address because it is formatted for display.
        AutofillAddress shippingAddress =
                new AutofillAddress(
                        context,
                        profile,
                        PersonalDataManagerFactory.getForProfile(
                                Profile.fromWebContents(mWebContents)));
        mDelegate.onShippingAddressChange(shippingAddress.toPaymentAddress());
    }

    // Implements PersonalDataManager.NormalizedAddressRequestDelegate:
    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        // Since the phone number is formatted in either case, this profile should be used.
        onAddressNormalized(profile);
    }

    private void startShippingAddressChangeNormalization(AutofillAddress address) {
        // Will call back into either onAddressNormalized or onCouldNotNormalize which will send the
        // result to the merchant.
        AddressNormalizerFactory.getInstance()
                .normalizeAddress(address.getProfile(), /* delegate= */ this);
    }

    /** @return Whether at least one payment app is available. */
    public boolean hasAvailableApps() {
        assert mHasInitialized;
        return mPaymentMethodsSection != null && !mPaymentMethodsSection.isEmpty();
    }

    /** Close the instance. Do not use this instance any more after calling this method. */
    public void close() {
        assert !mHasClosed;
        mHasClosed = true;

        if (mPaymentHandlerUi != null) {
            mPaymentHandlerUi.hide();
            mPaymentHandlerUi = null;
        }

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.close();
            ActivityLifecycleDispatcher dispatcher = mDelegate.getActivityLifecycleDispatcher();
            if (dispatcher != null) {
                dispatcher.unregister(mPaymentRequestUI);
            }
            mPaymentRequestUI = null;
            mPaymentUisShowStateReconciler.onPaymentRequestUiClosed();
        }
        if (mPaymentMethodsSection != null) {
            for (PaymentApp app : getPaymentApps()) {
                app.dismissInstrument();
            }
            mPaymentMethodsSection = null;
        }

        SettingsAutofillAndPaymentsObserver.getInstance().unregisterObserver(this);

        removeLeavingTabObservers();
        destroyCurrencyFormatters();
    }
}
