// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentAppCreator;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.ChromePaymentRequestService;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.PaymentPreferencesUtil;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.payments.ShippingStrings;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.FocusChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.autofill.Completable;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.BasicCardUtils;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentOptionsUtils;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.components.payments.PaymentUIsObserver;
import org.chromium.components.payments.PaymentUiServiceTestInterface;
import org.chromium.components.payments.Section;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
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
public class PaymentUiService implements SettingsAutofillAndPaymentsObserver.Observer,
                                         PaymentHandlerUiObserver, FocusChangedObserver,
                                         PaymentUiServiceTestInterface,
                                         NormalizedAddressRequestDelegate, PaymentRequestUI.Client {
    /** Limit in the number of suggested items in a section. */
    /* package */ static final int SUGGESTIONS_LIMIT = 4;

    // Reverse order of the comparator to sort in descending order of completeness scores.
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            (a, b) -> (PaymentAppComparator.compareCompletablesByCompleteness(b, a));
    private final Comparator<PaymentApp> mPaymentAppComparator;

    private final boolean mIsOffTheRecord;
    private final Handler mHandler = new Handler();
    private final Queue<Runnable> mRetryQueue = new LinkedList<>();
    private final OverviewModeObserver mOverviewModeObserver;
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
    private final CardEditor mCardEditor;
    private final PaymentUisShowStateReconciler mPaymentUisShowStateReconciler;
    private final PaymentRequestParams mParams;
    private final JourneyLogger mJourneyLogger;
    private final PaymentUIsObserver mObserver;

    private PaymentRequestUI mPaymentRequestUI;

    private ShoppingCart mUiShoppingCart;
    private boolean mMerchantSupportsAutofillCards;
    private boolean mHasInitialized;
    private boolean mHasClosed;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private AutofillPaymentAppCreator mAutofillPaymentAppCreator;
    private boolean mHaveRequestedAutofillData = true;
    private List<AutofillProfile> mAutofillProfiles;
    private boolean mCanUserAddCreditCard;
    private TabModelSelector mObservedTabModelSelector;
    private TabModel mObservedTabModel;
    private OverviewModeBehavior mOverviewModeBehavior;
    private MinimalUICoordinator mMinimalUi;

    /**
     * True if we should skip showing PaymentRequest UI.
     *
     * <p>In cases where there is a single payment app and the merchant does not request shipping
     * or billing, we can skip showing UI as Payment Request UI is not benefiting the user at all.
     */
    private boolean mShouldSkipShowingPaymentRequestUi;

    /** The delegate of this class. */
    public interface Delegate {
        /** Dispatch the payer detail change event if needed. */
        void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail);
        /** Record the show event to the journey logger and record the transaction amount. */
        void recordShowEventAndTransactionAmount();
        /**
         * @return Whether {@link ChromePaymentRequestService#retry} has been
         *         called.
         */
        boolean wasRetryCalled();

        // TODO(crbug.com/1144165): The return semantics is not intuitive for this method; the
        // method should not take the selectedShippingAddress, selectedShippingOption parameters of
        // UI and autofill semantics.
        /**
         * Invokes the selected payment app.
         * @param selectedShippingAddress the shipping address selected from the payment request UI.
         * @param selectedShippingOption The shipping option selected from the payment request UI.
         * @param selectedPaymentApp The selected payment app.
         * @return Whether the spinner should be displayed. Autofill cards show a CVC prompt, so the
         *         spinner is hidden in that case. Other payment apps typically show a spinner.
         */
        boolean invokePaymentApp(EditableOption selectedShippingAddress,
                EditableOption selectedShippingOption, PaymentApp selectedPaymentApp);

        /**
         * Invoked when the UI service has been aborted.
         * @param reason The reason for the aborting, as defined by {@link AbortReason}.
         * @param debugMessage The debug message for the aborting.
         */
        void onUiAborted(@AbortReason int reason, String debugMessage);
    }

    /**
     * This class is to coordinate the show state of a bottom sheet UI (either expandable payment
     * handler or minimal UI) and the Payment Request UI so that these visibility rules are
     * enforced:
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
        public void onPaymentRequestUiClosed() {
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
                mObserver.onUiServiceError(ErrorStrings.FAIL_TO_SHOW_PAYMENT_REQUEST_UI);
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
     * @param observer The payment UIs observer.
     */
    public PaymentUiService(Delegate delegate, PaymentRequestParams params, WebContents webContents,
            boolean isOffTheRecord, JourneyLogger journeyLogger, String topLevelOrigin,
            PaymentUIsObserver observer) {
        assert !params.hasClosed();
        mDelegate = delegate;
        mParams = params;

        // Do not persist changes on disk in OffTheRecord mode.
        mAddressEditor = new AddressEditor(
                AddressEditor.Purpose.PAYMENT_REQUEST, /*saveToDisk=*/!isOffTheRecord);
        // PaymentRequest card editor does not show the organization name in the dropdown with the
        // billing address labels.
        mCardEditor = new CardEditor(webContents, mAddressEditor, /*includeOrgLabel=*/false);
        mJourneyLogger = journeyLogger;
        mWebContents = webContents;
        mTopLevelOriginFormattedForDisplay = topLevelOrigin;
        mMerchantName = webContents.getTitle();

        mPaymentUisShowStateReconciler = new PaymentUisShowStateReconciler();
        mCurrencyFormatterMap = new HashMap<>();
        mIsOffTheRecord = isOffTheRecord;
        mPaymentAppComparator = new PaymentAppComparator(/*params=*/mParams);
        mObserver = observer;
        mSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mObserver.onLeavingCurrentTab(ErrorStrings.TAB_SWITCH);
            }
        };
        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (tab == null || tab.getId() != lastId) {
                    mObserver.onLeavingCurrentTab(ErrorStrings.TAB_SWITCH);
                }
            }
        };
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mObserver.onLeavingCurrentTab(ErrorStrings.TAB_OVERVIEW_MODE);
            }
        };
    }

    /** @return The PaymentRequestUI. */
    public PaymentRequestUI getPaymentRequestUI() {
        return mPaymentRequestUI;
    }

    /**
     * @return Whether the merchant supports autofill cards. It can be used only after
     *         initialize() is invoked.
     */
    public boolean merchantSupportsAutofillCards() {
        assert mHasInitialized;
        return mMerchantSupportsAutofillCards;
    }

    /** @return Get the PaymentMethodsSection of the PaymentRequest UI. */
    public SectionInformation getPaymentMethodsSection() {
        return mPaymentMethodsSection;
    }

    /** Set the PaymentMethodsSection of the PaymentRequest UI. */
    public void setPaymentMethodsSection(SectionInformation paymentMethodsSection) {
        mPaymentMethodsSection = paymentMethodsSection;
    }

    /** Get the ShippingAddressesSection of the PaymentRequest UI. */
    public SectionInformation getShippingAddressesSection() {
        return mShippingAddressesSection;
    }

    /** Get the ContactSection of the PaymentRequest UI. */
    public ContactDetailsSection getContactSection() {
        return mContactSection;
    }

    /** Set the AutofillPaymentAppCreator. */
    public void setAutofillPaymentAppCreator(AutofillPaymentAppCreator autofillPaymentAppCreator) {
        mAutofillPaymentAppCreator = autofillPaymentAppCreator;
    }

    /**
     * @return Whether user can add credit card. It can be used only after
     *         initialize() is invoked.
     */
    public boolean canUserAddCreditCard() {
        assert mHasInitialized;
        return mCanUserAddCreditCard;
    }

    /**
     * The UI model of the shopping cart, including the total. Each item includes a label and a
     * price string. This data is passed to the UI.
     */
    // Implements PaymentUiService.Delegate:
    @Override
    public void getShoppingCart(Callback<ShoppingCart> callback) {
        mHandler.post(callback.bind(mUiShoppingCart));
    }

    /**
     * The UI model for the shipping options. Includes the label and sublabel for each shipping
     * option. Also keeps track of the selected shipping option. This data is passed to the UI.
     */
    public SectionInformation getUiShippingOptions() {
        return mUiShippingOptions;
    }

    /** Get the contact editor on PaymentRequest UI. */
    public ContactEditor getContactEditor() {
        return mContactEditor;
    }

    /** @return The autofill profiles. */
    public List<AutofillProfile> getAutofillProfiles() {
        return mAutofillProfiles;
    }

    /** @return Whether PaymentRequestUI has requested autofill data. */
    public boolean haveRequestedAutofillData() {
        return mHaveRequestedAutofillData;
    }

    /** @return The minimal UI coordinator. */
    public MinimalUICoordinator getMinimalUI() {
        return mMinimalUi;
    }

    /** @return Whether the Payment Request or Minimal UIs are showing. */
    public boolean isShowingUI() {
        return mPaymentRequestUI != null || mMinimalUi != null;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean confirmMinimalUIForTest() {
        if (mMinimalUi == null) return false;
        mMinimalUi.confirmForTest();
        return true;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean dismissMinimalUIForTest() {
        if (mMinimalUi == null) return false;
        mMinimalUi.dismissForTest();
        return true;
    }

    /**
     * Triggers the minimal UI.
     * @param chromeActivity The Android activity for the Chrome UI that will host the minimal UI.
     * @param mRawTotal The raw total of the payment item.
     * @param readyObserver The onMinimalUIReady function.
     * @param confirmObserver The onMinimalUiConfirmed function.
     * @param dismissObserver The onMinimalUiDismissed function.
     */
    public boolean triggerMinimalUI(ChromeActivity chromeActivity, PaymentItem mRawTotal,
            MinimalUICoordinator.ReadyObserver readyObserver,
            MinimalUICoordinator.ConfirmObserver confirmObserver,
            MinimalUICoordinator.DismissObserver dismissObserver) {
        // Do not show the Payment Request UI dialog even if the minimal UI is suppressed.
        mPaymentUisShowStateReconciler.onBottomSheetShown();
        mMinimalUi = new MinimalUICoordinator();
        return mMinimalUi.show(chromeActivity,
                BottomSheetControllerProvider.from(chromeActivity.getWindowAndroid()),
                (PaymentApp) mPaymentMethodsSection.getSelectedItem(),
                mCurrencyFormatterMap.get(mRawTotal.amount.currency), mUiShoppingCart.getTotal(),
                readyObserver, confirmObserver, dismissObserver);
    }

    /**
     * Called when the payment request is complete.
     * @param result The status of PaymentComplete.
     * @param onMinimalUiErroredAndClosed The function called when MinimalUI errors and closes.
     * @param onMinimalUiCompletedAndClosed The function called when MinimalUI completes and closes.
     * @param onPaymentRequestCompleteForNonMinimalUI The function called when PaymentRequest
     *                                                completes for non-minimal UI.
     */
    public void onPaymentRequestComplete(int result,
            MinimalUICoordinator.ErrorAndCloseObserver onMinimalUiErroredAndClosed,
            MinimalUICoordinator.CompleteAndCloseObserver onMinimalUiCompletedAndClosed,
            Runnable onPaymentRequestCompleteForNonMinimalUI) {
        // Update records of the used payment app for sorting payment apps next time.
        EditableOption selectedPaymentMethod = mPaymentMethodsSection.getSelectedItem();
        PaymentPreferencesUtil.increasePaymentAppUseCount(selectedPaymentMethod.getIdentifier());
        PaymentPreferencesUtil.setPaymentAppLastUseDate(
                selectedPaymentMethod.getIdentifier(), System.currentTimeMillis());

        if (mMinimalUi != null) {
            mMinimalUi.onPaymentRequestComplete(
                    result, onMinimalUiErroredAndClosed, onMinimalUiCompletedAndClosed);
            return;
        }

        onPaymentRequestCompleteForNonMinimalUI.run();
    }

    /** @return Whether PaymentRequestUI should be skipped. */
    public boolean shouldSkipShowingPaymentRequestUi() {
        return mShouldSkipShowingPaymentRequestUi;
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onAddressUpdated(AutofillAddress address) {
        address.setShippingAddressLabelWithCountry();
        mCardEditor.updateBillingAddressIfComplete(address);

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

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onCreditCardUpdated(CreditCard card) {
        assert mHasInitialized;
        if (!mMerchantSupportsAutofillCards || mPaymentMethodsSection == null
                || mAutofillPaymentAppCreator == null) {
            return;
        }

        PaymentApp updatedAutofillCard = mAutofillPaymentAppCreator.createPaymentAppForCard(card);

        // Can be null when the card added through settings does not match the requested card
        // network or is invalid, because autofill settings do not perform the same level of
        // validation as Basic Card implementation in Chrome.
        if (updatedAutofillCard == null) return;

        mPaymentMethodsSection.addAndSelectOrUpdateItem(updatedAutofillCard);

        updateAppModifiedTotals();

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    // Implement SettingsAutofillAndPaymentsObserver.Observer:
    @Override
    public void onCreditCardDeleted(String guid) {
        assert mHasInitialized;
        if (!mMerchantSupportsAutofillCards || mPaymentMethodsSection == null) return;

        mPaymentMethodsSection.removeAndUnselectItem(guid);

        updateAppModifiedTotals();

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateSection(
                    PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    /**
     * Initializes the payment UI service.
     * @param details The PaymentDetails provided by the merchant.
     */
    public void initialize(PaymentDetails details) {
        assert !mParams.hasClosed();
        updateDetailsOnPaymentRequestUI(details);
        for (PaymentMethodData method : mParams.getMethodData().values()) {
            mCardEditor.addAcceptedPaymentMethodIfRecognized(method);
        }
        // Checks whether the merchant supports autofill cards before show is called.
        mMerchantSupportsAutofillCards =
                BasicCardUtils.merchantSupportsBasicCard(mParams.getMethodData());

        // If in strict mode, don't give user an option to add an autofill card during the checkout
        // to avoid the "unhappy" basic-card flow.
        mCanUserAddCreditCard = mMerchantSupportsAutofillCards
                && !PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.STRICT_HAS_ENROLLED_AUTOFILL_INSTRUMENT);

        if (PaymentOptionsUtils.requestAnyInformation(mParams.getPaymentOptions())) {
            mAutofillProfiles = Collections.unmodifiableList(
                    PersonalDataManager.getInstance().getProfilesToSuggest(
                            false /* includeNameInLabel */));
        }

        if (mParams.getPaymentOptions().requestShipping) {
            boolean haveCompleteShippingAddress = false;
            for (int i = 0; i < mAutofillProfiles.size(); i++) {
                if (AutofillAddress.checkAddressCompletionStatus(
                            mAutofillProfiles.get(i), AutofillAddress.CompletenessCheckType.NORMAL)
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
            mContactEditor = new ContactEditor(options.requestPayerName, options.requestPayerPhone,
                    options.requestPayerEmail,
                    /*saveToDisk=*/!mIsOffTheRecord);
            boolean haveCompleteContactInfo = false;
            for (int i = 0; i < getAutofillProfiles().size(); i++) {
                AutofillProfile profile = getAutofillProfiles().get(i);
                if (getContactEditor().checkContactCompletionStatus(profile.getFullName(),
                            profile.getPhoneNumber(), profile.getEmailAddress())
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
     * @param errors The payment validation errors.
     */
    public void onRetry(PaymentValidationErrors errors) {
        // Remove all payment apps except the selected one.
        assert mPaymentMethodsSection != null;
        PaymentApp selectedApp = (PaymentApp) mPaymentMethodsSection.getSelectedItem();
        assert selectedApp != null;
        mPaymentMethodsSection = new SectionInformation(PaymentRequestUI.DataType.PAYMENT_METHODS,
                /* selection = */ 0, new ArrayList<>(Arrays.asList(selectedApp)));
        mPaymentRequestUI.updateSection(
                PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        mPaymentRequestUI.disableAddingNewCardsDuringRetry();

        // Go back to the payment sheet
        mPaymentRequestUI.onPayButtonProcessingCancelled();
        PaymentDetailsUpdateServiceHelper.getInstance().reset();
        if (!TextUtils.isEmpty(errors.error)) {
            mPaymentRequestUI.setRetryErrorMessage(errors.error);
        } else {
            ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
            mPaymentRequestUI.setRetryErrorMessage(
                    activity.getResources().getString(R.string.payments_error_message));
        }

        if (shouldShowShippingSection() && hasShippingAddressError(errors.shippingAddress)) {
            mRetryQueue.add(() -> {
                mAddressEditor.setAddressErrors(errors.shippingAddress);
                AutofillAddress selectedAddress =
                        (AutofillAddress) mShippingAddressesSection.getSelectedItem();
                editAddress(selectedAddress);
            });
        }

        if (shouldShowContactSection() && hasPayerError(errors.payer)) {
            mRetryQueue.add(() -> {
                mContactEditor.setPayerErrors(errors.payer);
                AutofillContact selectedContact =
                        (AutofillContact) mContactSection.getSelectedItem();
                editContactOnPaymentRequestUI(selectedContact);
            });
        }

        if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
    }

    private boolean hasShippingAddressError(AddressErrors errors) {
        return !TextUtils.isEmpty(errors.addressLine) || !TextUtils.isEmpty(errors.city)
                || !TextUtils.isEmpty(errors.country)
                || !TextUtils.isEmpty(errors.dependentLocality)
                || !TextUtils.isEmpty(errors.organization) || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.postalCode) || !TextUtils.isEmpty(errors.recipient)
                || !TextUtils.isEmpty(errors.region) || !TextUtils.isEmpty(errors.sortingCode);
    }

    private boolean hasPayerError(PayerErrors errors) {
        return !TextUtils.isEmpty(errors.name) || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.email);
    }

    /** @return The selected payment app type. */
    public @PaymentAppType int getSelectedPaymentAppType() {
        return mPaymentMethodsSection != null && mPaymentMethodsSection.getSelectedItem() != null
                ? ((PaymentApp) mPaymentMethodsSection.getSelectedItem()).getPaymentAppType()
                : PaymentAppType.UNDEFINED;
    }

    /** Sets the modifier for the order summary based on the given app, if any. */
    public void updateOrderSummary(@Nullable PaymentApp app) {
        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_MODIFIERS)) return;
        if (mParams.hasClosed()) return;
        PaymentDetailsModifier modifier = getModifier(app);
        PaymentItem total = modifier == null ? null : modifier.total;
        if (total == null) total = mParams.getRawTotal();

        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(total.amount);
        mUiShoppingCart.setTotal(new LineItem(total.label, formatter.getFormattedCurrencyCode(),
                formatter.format(total.amount.value), false /* isPending */));
        mUiShoppingCart.setAdditionalContents(modifier == null
                        ? null
                        : getLineItems(Arrays.asList(modifier.additionalDisplayItems)));
        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.updateOrderSummarySection(mUiShoppingCart);
        }
    }

    /** @return The first modifier that matches the given app, or null. */
    @Nullable
    private PaymentDetailsModifier getModifier(@Nullable PaymentApp app) {
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
    public void updateAppModifiedTotals() {
        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_MODIFIERS)) return;
        if (mParams.hasClosed() || mParams.getMethodData().isEmpty()) return;
        if (mPaymentMethodsSection == null) return;

        for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
            PaymentApp app = (PaymentApp) mPaymentMethodsSection.getItem(i);
            PaymentDetailsModifier modifier = getModifier(app);
            app.setModifiedTotal(modifier == null || modifier.total == null
                            ? null
                            : getOrCreateCurrencyFormatter(modifier.total.amount)
                                      .format(modifier.total.amount.value));
        }

        updateOrderSummary((PaymentApp) mPaymentMethodsSection.getSelectedItem());
    }

    /**
     * Gets currency formatter for a given PaymentCurrencyAmount,
     * creates one if no existing instance is found.
     *
     * @param amount The given payment amount.
     */
    public CurrencyFormatter getOrCreateCurrencyFormatter(PaymentCurrencyAmount amount) {
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
    public List<LineItem> getLineItems(@Nullable List<PaymentItem> items) {
        // Line items are optional.
        if (items == null) return new ArrayList<>();

        List<LineItem> result = new ArrayList<>(items.size());
        for (int i = 0; i < items.size(); i++) {
            PaymentItem item = items.get(i);
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(item.amount);
            result.add(new LineItem(item.label,
                    isMixedOrChangedCurrency() ? formatter.getFormattedCurrencyCode() : "",
                    formatter.format(item.amount.value), item.pending));
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
    public SectionInformation getShippingOptions(@Nullable PaymentShippingOption[] options) {
        // Shipping options are optional.
        if (options == null || options.length == 0) {
            return new SectionInformation(PaymentRequestUI.DataType.SHIPPING_OPTIONS);
        }

        List<EditableOption> result = new ArrayList<>();
        int selectedItemIndex = SectionInformation.NO_SELECTION;
        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(option.amount);
            String currencyPrefix = isMixedOrChangedCurrency()
                    ? formatter.getFormattedCurrencyCode() + "\u0020"
                    : "";
            result.add(new EditableOption(option.id, option.label,
                    currencyPrefix + formatter.format(option.amount.value), null));
            if (option.selected) selectedItemIndex = i;
        }

        return new SectionInformation(PaymentRequestUI.DataType.SHIPPING_OPTIONS, selectedItemIndex,
                Collections.unmodifiableList(result));
    }

    /** Destroy the currency formatters. */
    public void destroyCurrencyFormatters() {
        for (CurrencyFormatter formatter : mCurrencyFormatterMap.values()) {
            assert formatter != null;
            // Ensures the native implementation of currency formatter does not leak.
            formatter.destroy();
        }
        mCurrencyFormatterMap.clear();
    }

    /**
     * Notifies the UI about the changes in selected payment method.
     */
    public void onSelectedPaymentMethodUpdated() {
        mPaymentRequestUI.selectedPaymentMethodUpdated(
                new PaymentInformation(mUiShoppingCart, mShippingAddressesSection,
                        mUiShippingOptions, mContactSection, mPaymentMethodsSection));
    }

    /**
     * Update Payment Request UI with the update event's information and enable the UI. This method
     * should be called when the user interface is disabled with a "↻" spinner being displayed. The
     * user is unable to interact with the user interface until this method is called.
     * @return Whether this is the first time that payment information has been provided to the user
     *         interface, which indicates that the "UI shown" event should be recorded now.
     */
    public boolean enableAndUpdatePaymentRequestUIWithPaymentInfo() {
        boolean isFirstUpdate = false;
        if (mPaymentInformationCallback != null && mPaymentMethodsSection != null) {
            providePaymentInformationToPaymentRequestUI();
            isFirstUpdate = true;
        } else {
            mPaymentRequestUI.updateOrderSummarySection(mUiShoppingCart);
            if (shouldShowShippingSection()) {
                mPaymentRequestUI.updateSection(
                        PaymentRequestUI.DataType.SHIPPING_OPTIONS, mUiShippingOptions);
            }
        }
        return isFirstUpdate;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean shouldShowShippingSection() {
        if (mParams.hasClosed() || !mParams.getPaymentOptions().requestShipping) return false;

        if (mPaymentMethodsSection == null) return true;

        PaymentApp selectedApp = (PaymentApp) mPaymentMethodsSection.getSelectedItem();
        return selectedApp == null || !selectedApp.handlesShippingAddress();
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean shouldShowContactSection() {
        PaymentApp selectedApp = (mPaymentMethodsSection == null)
                ? null
                : (PaymentApp) mPaymentMethodsSection.getSelectedItem();
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
     * @param url The URL of the payment app.
     * @param isOffTheRecord Whether the merchant page is currently in an OffTheRecord tab.
     * @return The WebContents of the payment handler that's just opened when the opening is
     *         successful; null if failed.
     */
    @Nullable
    public WebContents showPaymentHandlerUI(GURL url, boolean isOffTheRecord) {
        if (mPaymentHandlerUi != null) return null;
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) return null;

        PaymentHandlerCoordinator paymentHandlerUi = new PaymentHandlerCoordinator();
        WebContents paymentHandlerWebContents = paymentHandlerUi.show(
                /*paymentRequestWebContents=*/mWebContents, url, isOffTheRecord,
                /*uiObserver=*/this);
        if (paymentHandlerWebContents != null) mPaymentHandlerUi = paymentHandlerUi;
        return paymentHandlerWebContents;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public WebContents getPaymentHandlerWebContentsForTest() {
        if (mPaymentHandlerUi == null) return null;
        return mPaymentHandlerUi.getWebContentsForTest();
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean clickPaymentHandlerSecurityIconForTest() {
        if (mPaymentHandlerUi == null) return false;
        mPaymentHandlerUi.clickSecurityIconForTest();
        return true;
    }

    // Implements PaymentUiServiceTestInterface:
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean clickPaymentHandlerCloseButtonForTest() {
        if (mPaymentHandlerUi == null) return false;
        mPaymentHandlerUi.clickCloseButtonForTest();
        return true;
    }

    /** Provide PaymentInformation to the PaymentRequest UI. */
    public void providePaymentInformationToPaymentRequestUI() {
        // Do not display service worker payment apps summary in single line so as to display its
        // origin completely.
        mPaymentMethodsSection.setDisplaySelectedItemSummaryInSingleLineInNormalMode(
                getSelectedPaymentAppType() != PaymentAppType.SERVICE_WORKER_APP);
        mPaymentInformationCallback.onResult(
                new PaymentInformation(mUiShoppingCart, mShippingAddressesSection,
                        mUiShippingOptions, mContactSection, mPaymentMethodsSection));
        mPaymentInformationCallback = null;
    }

    /**
     * Edit the contact information on the PaymentRequest UI.
     * @param toEdit The information to edit, allowed to be null.
     **/
    public void editContactOnPaymentRequestUI(@Nullable final AutofillContact toEdit) {
        mContactEditor.edit(toEdit, new Callback<AutofillContact>() {
            @Override
            public void onResult(AutofillContact editedContact) {
                if (mPaymentRequestUI == null) return;

                if (editedContact != null) {
                    mContactEditor.setPayerErrors(null);

                    // A partial or complete contact came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedContact.isComplete()) {
                        // If the contact is not complete according to the requirements of the flow,
                        // unselect it (editor can return incomplete information when cancelled).
                        mContactSection.setSelectedItemIndex(SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Contact is complete and we were in the "Add flow": add an item to the
                        // list.
                        mContactSection.addAndSelectItem(editedContact);
                    } else {
                        mDelegate.dispatchPayerDetailChangeEventIfNeeded(
                                editedContact.toPayerDetail());
                    }
                    // If contact is complete and (toEdit != null), no action needed: the contact
                    // was already selected in the UI.
                }
                // If |editedContact| is null, the user has cancelled out of the "Add flow". No
                // action to take (if a contact was selected in the UI, it will stay selected).

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
    public void editAddress(@Nullable final AutofillAddress toEdit) {
        mAddressEditor.edit(toEdit, new Callback<AutofillAddress>() {
            @Override
            public void onResult(AutofillAddress editedAddress) {
                if (mPaymentRequestUI == null) return;

                if (editedAddress != null) {
                    mAddressEditor.setAddressErrors(null);

                    // Sets or updates the shipping address label.
                    editedAddress.setShippingAddressLabelWithCountry();

                    mCardEditor.updateBillingAddressIfComplete(editedAddress);

                    // A partial or complete address came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedAddress.isComplete()) {
                        // If the address is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mShippingAddressesSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                        providePaymentInformationToPaymentRequestUI();
                        mDelegate.recordShowEventAndTransactionAmount();
                    } else {
                        if (toEdit == null) {
                            // Address is complete and user was in the "Add flow": add an item to
                            // the list.
                            mShippingAddressesSection.addAndSelectItem(editedAddress);
                        }

                        if (mContactSection != null) {
                            // Update |mContactSection| with the new/edited
                            // address, which will update an existing item or add a new one to the
                            // end of the list.
                            mContactSection.addOrUpdateWithAutofillAddress(editedAddress);
                            mPaymentRequestUI.updateSection(
                                    PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);
                        }

                        startShippingAddressChangeNormalization(editedAddress);
                    }
                } else {
                    providePaymentInformationToPaymentRequestUI();
                    mDelegate.recordShowEventAndTransactionAmount();
                }

                if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
            }
        });
    }

    /**
     * Build the PaymentRequest UI.
     * @param activity The ChromeActivity for the payment request, cannot be null.
     * @param isWebContentsActive Whether the merchant's WebContents is active.
     * @param isShowWaitingForUpdatedDetails Whether showing payment app or the app selector is
     *         blocked on the updated payment details.
     * @return The error message if built unsuccessfully; null otherwise.
     */
    @Nullable
    public String buildPaymentRequestUI(ChromeActivity activity, boolean isWebContentsActive,
            boolean isShowWaitingForUpdatedDetails) {
        // Payment methods section must be ready before building the rest of the UI. This is because
        // shipping and contact sections (when requested by merchant) are populated depending on
        // whether or not the selected payment app (if such exists) can provide the required
        // information.
        assert mPaymentMethodsSection != null;

        assert activity != null;

        // Only the currently selected tab is allowed to show the payment UI.
        if (!isWebContentsActive) return ErrorStrings.CANNOT_SHOW_IN_BACKGROUND_TAB;

        if (mParams.hasClosed()) return ErrorStrings.PAYMENT_REQUEST_IS_ABORTING;

        // Catch any time the user switches tabs. Because the dialog is modal, a user shouldn't be
        // allowed to switch tabs, which can happen if the user receives an external Intent.
        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
        }
        mObservedTabModelSelector = activity.getTabModelSelector();
        mObservedTabModelSelector.addObserver(mSelectorObserver);
        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
        }
        mObservedTabModel = activity.getCurrentTabModel();
        mObservedTabModel.addObserver(mTabModelObserver);

        // Catch any time the user enters the overview mode and dismiss the payment UI.
        if (activity instanceof ChromeTabbedActivity) {
            if (mOverviewModeBehavior != null) {
                mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            }
            mOverviewModeBehavior =
                    ((ChromeTabbedActivity) activity).getOverviewModeBehaviorSupplier().get();

            assert mOverviewModeBehavior != null;
            if (mOverviewModeBehavior.overviewVisible()) return ErrorStrings.TAB_OVERVIEW_MODE;
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }

        if (shouldShowShippingSection() && !isShowWaitingForUpdatedDetails) {
            createShippingSectionForPaymentRequestUI(activity);
        }

        if (shouldShowContactSection()) {
            mContactSection = new ContactDetailsSection(
                    activity, mAutofillProfiles, mContactEditor, mJourneyLogger);
        }

        mPaymentRequestUI = new PaymentRequestUI(activity, /*client=*/this,
                mMerchantSupportsAutofillCards, !PaymentPreferencesUtil.isPaymentCompleteOnce(),
                mMerchantName, mTopLevelOriginFormattedForDisplay,
                SecurityStateModel.getSecurityLevelForWebContents(mWebContents),
                new ShippingStrings(mParams.getPaymentOptions().shippingType),
                mPaymentUisShowStateReconciler, Profile.fromWebContents(mWebContents));
        activity.getLifecycleDispatcher().register(
                mPaymentRequestUI); // registered as a PauseResumeWithNativeObserver

        final FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(Profile.fromWebContents(mWebContents),
                mWebContents.getLastCommittedUrl(),
                activity.getResources().getDimensionPixelSize(R.dimen.payments_favicon_size),
                (bitmap, iconUrl) -> {
                    if (bitmap == null) {
                        mObserver.onPaymentRequestUIFaviconNotAvailable();
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
        mCardEditor.setEditorDialog(mPaymentRequestUI.getCardEditorDialog());
        if (mContactEditor != null) {
            mContactEditor.setEditorDialog(mPaymentRequestUI.getEditorDialog());
        }
        return null;
    }

    /** Create a shipping section for PaymentRequest UI. */
    public void createShippingSectionForPaymentRequestUI(Context context) {
        List<AutofillAddress> addresses = new ArrayList<>();

        for (int i = 0; i < mAutofillProfiles.size(); i++) {
            AutofillProfile profile = mAutofillProfiles.get(i);
            mAddressEditor.addPhoneNumberIfValid(profile.getPhoneNumber());

            // Only suggest addresses that have a street address.
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                addresses.add(new AutofillAddress(context, profile));
            }
        }

        // Suggest complete addresses first.
        Collections.sort(addresses, COMPLETENESS_COMPARATOR);

        // Limit the number of suggestions.
        addresses = addresses.subList(0, Math.min(addresses.size(), SUGGESTIONS_LIMIT));

        // Load the validation rules for each unique region code.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < addresses.size(); ++i) {
            String countryCode = AutofillAddress.getCountryCode(addresses.get(i).getProfile());
            if (!uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForAddressNormalization(countryCode);
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

        int missingFields = 0;
        if (addresses.isEmpty()) {
            // All fields are missing.
            missingFields = AutofillAddress.CompletionStatus.INVALID_RECIPIENT
                    | AutofillAddress.CompletionStatus.INVALID_PHONE_NUMBER
                    | AutofillAddress.CompletionStatus.INVALID_ADDRESS;
        } else {
            missingFields = addresses.get(0).getMissingFieldsOfShippingProfile();
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingShippingFields", missingFields);
        }

        mShippingAddressesSection = new SectionInformation(
                PaymentRequestUI.DataType.SHIPPING_ADDRESSES, firstCompleteAddressIndex, addresses);
    }

    /**
     * Rank the payment apps for PaymentRequest UI.
     * @param paymentApps A list of payment apps to be ranked in place.
     */
    public void rankPaymentAppsForPaymentRequestUI(List<PaymentApp> paymentApps) {
        Collections.sort(paymentApps, mPaymentAppComparator);
    }

    /**
     * Edit the credit cards on the PaymentRequest UI.
     * @param toEdit The AutofillPaymentInstrument whose credit card is to replace those on the UI,
     *         allowed to be null.
     */
    public void editCard(@Nullable final AutofillPaymentInstrument toEdit) {
        mCardEditor.edit(toEdit, new Callback<AutofillPaymentInstrument>() {
            @Override
            public void onResult(AutofillPaymentInstrument editedCard) {
                if (mPaymentRequestUI == null) return;

                if (editedCard != null) {
                    // A partial or complete card came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedCard.isComplete()) {
                        // If the card is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mPaymentMethodsSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Card is complete and we were in the "Add flow": add an item to the list.
                        mPaymentMethodsSection.addAndSelectItem(editedCard);
                    }
                    // If card is complete and (toEdit != null), no action needed: the card was
                    // already selected in the UI.
                }
                // If |editedCard| is null, the user has cancelled out of the "Add flow". No action
                // to take (if another card was selected prior to the add flow, it will stay
                // selected).

                updateAppModifiedTotals();
                mPaymentRequestUI.updateSection(
                        PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
            }
        });
    }

    // Implements PaymentRequestUi.Delegate:
    @Override
    public void getSectionInformation(@PaymentRequestUI.DataType final int optionType,
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

    // Implements PaymentUiService.Delegate:
    @Override
    public boolean onPayClicked(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, EditableOption selectedPaymentMethod) {
        return mDelegate.invokePaymentApp(selectedShippingAddress, selectedShippingOption,
                (PaymentApp) selectedPaymentMethod);
    }

    // Implements PaymentUiService.Delegate:
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
            editCard(null);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionEditOption(@PaymentRequestUI.DataType int optionType, EditableOption option,
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
            editCard((AutofillPaymentInstrument) option);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        assert false;
        return PaymentRequestUI.SelectionResult.NONE;
    }

    /** Set a change observer for the shipping address section on the PaymentRequest UI. */
    public void setShippingAddressSectionFocusChangedObserverForPaymentRequestUI() {
        mPaymentRequestUI.setShippingAddressSectionFocusChangedObserver(this);
    }

    /**
     * @param options The payment options specified in the payment request.
     * @return true when there is exactly one available payment app which can provide all requested
     * information including shipping address and payer's contact information whenever needed.
     */
    public boolean onlySingleAppCanProvideAllRequiredInformation(PaymentOptions options) {
        assert mPaymentMethodsSection != null;

        if (!PaymentOptionsUtils.requestAnyInformation(options)) {
            return mPaymentMethodsSection.getSize() == 1
                    && !((PaymentApp) mPaymentMethodsSection.getItem(0)).isAutofillInstrument();
        }

        boolean anAppCanProvideAllInfo = false;
        int sectionSize = mPaymentMethodsSection.getSize();
        for (int i = 0; i < sectionSize; i++) {
            PaymentApp app = (PaymentApp) mPaymentMethodsSection.getItem(i);
            if ((!options.requestShipping || app.handlesShippingAddress())
                    && (!options.requestPayerName || app.handlesPayerName())
                    && (!options.requestPayerPhone || app.handlesPayerPhone())
                    && (!options.requestPayerEmail || app.handlesPayerEmail())) {
                // There is more than one available app that can provide all merchant requested
                // information information.
                if (anAppCanProvideAllInfo) return false;

                anAppCanProvideAllInfo = true;
            }
        }
        return anAppCanProvideAllInfo;
    }

    /**
     * Update the details related fields on the PaymentRequest UI.
     * @param details The details whose information is used for the update.
     */
    public void updateDetailsOnPaymentRequestUI(PaymentDetails details) {
        loadCurrencyFormattersForPaymentDetails(details);
        // Total is never pending.
        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(details.total.amount);
        LineItem uiTotal = new LineItem(details.total.label, formatter.getFormattedCurrencyCode(),
                formatter.format(details.total.amount.value), /*isPending=*/false);

        List<PaymentItem> itemList = details.displayItems == null
                ? new ArrayList<>()
                : Arrays.asList(details.displayItems);
        List<LineItem> uiLineItems = getLineItems(itemList);

        mUiShoppingCart = new ShoppingCart(uiTotal, uiLineItems);

        if (mUiShippingOptions == null || details.shippingOptions != null) {
            mUiShippingOptions = getShippingOptions(details.shippingOptions);
        }

        updateAppModifiedTotals();
    }

    /**
     * Calculate whether the browser payment sheet should be skipped directly into the payment app.
     * @param isUserGestureShow Whether the PaymentRequest.show() is triggered by user gesture.
     * @param urlPaymentMethodIdentifiersSupported True when at least one url payment method
     *         identifier is specified in payment request.
     * @param skipUiForNonUrlPaymentMethodIdentifiers True when skip UI is available for non-url
     *         based payment method identifiers (e.g., basic-card).
     * @param options The payment options specified in the payment request.
     */
    public void calculateWhetherShouldSkipShowingPaymentRequestUi(boolean isUserGestureShow,
            boolean urlPaymentMethodIdentifiersSupported,
            boolean skipUiForNonUrlPaymentMethodIdentifiers, PaymentOptions options) {
        assert mPaymentMethodsSection != null;
        PaymentApp selectedApp = (PaymentApp) mPaymentMethodsSection.getSelectedItem();

        // If there is only a single payment app which can provide all merchant requested
        // information, we can safely go directly to the payment app instead of showing Payment
        // Request UI.
        mShouldSkipShowingPaymentRequestUi =
                PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP)
                // Only allowing payment apps that own their own UIs.
                // This excludes AutofillPaymentInstrument as its UI is rendered inline in
                // the payment request UI, thus can't be skipped.
                && (urlPaymentMethodIdentifiersSupported || skipUiForNonUrlPaymentMethodIdentifiers)
                && mPaymentMethodsSection.getSize() >= 1
                && onlySingleAppCanProvideAllRequiredInformation(options)
                // Skip to payment app only if it can be pre-selected.
                && selectedApp != null
                // Skip to payment app only if user gesture is provided when it is required to
                // skip-UI.
                && (isUserGestureShow || !selectedApp.isUserGestureRequiredToSkipUi());
    }

    /** Removes all of the observers that observe users leaving the tab. */
    public void removeLeavingTabObservers() {
        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
            mObservedTabModelSelector = null;
        }

        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
            mObservedTabModel = null;
        }

        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
    }

    // Implements PaymentRequestUI.Client:
    @Override
    public void getDefaultPaymentInformation(
            boolean isShowWaitingForUpdatedDetails, Callback<PaymentInformation> callback) {
        mPaymentInformationCallback = callback;

        if (isShowWaitingForUpdatedDetails) return;

        mHandler.post(() -> {
            if (mPaymentRequestUI != null) {
                providePaymentInformationToPaymentRequestUI();
                mDelegate.recordShowEventAndTransactionAmount();
            }
        });
    }

    /**
     * The implementation of {@link PaymentRequestUI.Client#onSectionOptionSelected}.
     * @param optionType Data being updated.
     * @param option Value of the data being updated.
     * @param callback The callback after an asynchronous check has completed.
     * @return The result of the selection.
     */
    // Implements PaymentUiService.Delegate:
    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionOptionSelected(@PaymentRequestUI.DataType int optionType,
            EditableOption option, Callback<PaymentInformation> callback) {
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
            mObserver.onShippingOptionChange(option.getIdentifier());
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
                ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
                assert activity != null;
                createShippingSectionForPaymentRequestUI(activity);
            }
            if (shouldShowContactSection() && mContactSection == null) {
                ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
                assert activity != null;
                mContactSection = new ContactDetailsSection(
                        activity, mAutofillProfiles, mContactEditor, mJourneyLogger);
            }
            onSelectedPaymentMethodUpdated();
            PaymentApp paymentApp = (PaymentApp) option;
            if (paymentApp instanceof AutofillPaymentInstrument) {
                AutofillPaymentInstrument card = (AutofillPaymentInstrument) paymentApp;

                if (!card.isComplete()) {
                    editCard(card);
                    return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
                }
            }

            updateOrderSummary(paymentApp);
            mPaymentMethodsSection.setSelectedItem(option);
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void onDismiss() {
        mDelegate.onUiAborted(AbortReason.ABORTED_BY_USER, ErrorStrings.USER_CANCELLED);
    }

    // Implements PaymentUiService.Delegate:
    @Override
    public void onCardAndAddressSettingsClicked() {
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (context == null) {
            mDelegate.onUiAborted(AbortReason.OTHER, ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context);
    }

    // Implements PersonalDataManager.NormalizedAddressRequestDelegate:
    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);

        // Can happen if the tab is closed during the normalization process.
        if (chromeActivity == null) {
            mObserver.onUiServiceError(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        // Don't reuse the selected address because it is formatted for display.
        AutofillAddress shippingAddress = new AutofillAddress(chromeActivity, profile);
        mObserver.onShippingAddressChange(shippingAddress.toPaymentAddress());
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
        PersonalDataManager.getInstance().normalizeAddress(address.getProfile(), /*delegate=*/this);
    }

    /** @return Whether at least one payment app (including basic-card payment app) is available. */
    public boolean hasAvailableApps() {
        assert mHasInitialized;
        return (mPaymentMethodsSection != null && !mPaymentMethodsSection.isEmpty())
                || mMerchantSupportsAutofillCards;
    }

    /** Close the instance. Do not use this instance any more after calling this method. */
    public void close() {
        assert !mHasClosed;
        mHasClosed = true;

        if (mPaymentHandlerUi != null) {
            mPaymentHandlerUi.hide();
            mPaymentHandlerUi = null;
        }

        if (mMinimalUi != null) {
            mMinimalUi.hide();
            mMinimalUi = null;
        }

        if (mPaymentRequestUI != null) {
            mPaymentRequestUI.close();
            ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
            if (activity != null) {
                activity.getLifecycleDispatcher().unregister(mPaymentRequestUI);
            }
            mPaymentRequestUI = null;
            mPaymentUisShowStateReconciler.onPaymentRequestUiClosed();
        }
        if (mPaymentMethodsSection != null) {
            for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
                EditableOption option = mPaymentMethodsSection.getItem(i);
                ((PaymentApp) option).dismissInstrument();
            }
            mPaymentMethodsSection = null;
        }

        removeLeavingTabObservers();
        destroyCurrencyFormatters();
    }
}
