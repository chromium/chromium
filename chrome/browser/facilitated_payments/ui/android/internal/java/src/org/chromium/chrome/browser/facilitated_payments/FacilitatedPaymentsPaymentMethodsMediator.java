// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_ICON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_NUMBER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_PAYMENT_RAIL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TRANSACTION_LIMIT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TYPE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ErrorScreenProperties.PRIMARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ON_EWALLET_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PAYMENT_LINK_TITLE_TOP_MARGIN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.PRODUCT_ICON_HEIGHT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.SECURITY_CHECK_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.EWALLET;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.PAYMENT_APP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.ON_PAYMENT_APP_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_ICON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ACCEPT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.DECLINE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SURVIVES_NAVIGATION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PIX_ACCOUNT_LINKING_PROMPT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SWAPPING_SCREEN;
import static org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment.FINANCIAL_ACCOUNTS;
import static org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment.NON_CARD_PAYMENT_METHODS;
import static org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment.PAYMENT_METHODS;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.IconSpecs;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsComponent.Delegate;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FooterProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.facilitated_payments.core.ui_utils.FopSelectorAction;
import org.chromium.components.facilitated_payments.core.ui_utils.PaymentLinkFopSelectorAction;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Contains the logic for the facilitated payments component. It sets the state of the model and
 * reacts to events like clicks.
 */
@NullMarked
class FacilitatedPaymentsPaymentMethodsMediator {
    static final String PIX_BANK_ACCOUNT_TRANSACTION_LIMIT = "500";

    // This histogram name should be in sync with the one in
    // components/facilitated_payments/core/metrics/facilitated_payments_metrics.cc:LogPixFopSelected.
    @VisibleForTesting
    static final String PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM =
            "FacilitatedPayments.Pix.FopSelector.UserAction";

    // This histogram name should be in sync with the one in
    // components/facilitated_payments/core/metrics/facilitated_payments_metrics.cc:LogEwalletFopSelected.
    @VisibleForTesting
    static final String EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM =
            "FacilitatedPayments.Ewallet.FopSelector.UserAction.";

    // This histogram name should be in sync with the one in
    // components/facilitated_payments/core/metrics/facilitated_payments_metrics.cc:LogNonCardPaymentMethodsFopSelected.
    static final String PAYMENT_LINK_FOP_SELECTOR_TYPES_USER_ACTION_HISTOGRAM =
            "FacilitatedPayments.{PaymentLinkFopSelectorTypes}.FopSelector.UserAction";

    private Context mContext;
    private PropertyModel mModel;
    private Delegate mDelegate;
    private Profile mProfile;
    private InputProtector mInputProtector = new InputProtector();

    @Initializer
    void initialize(Context context, PropertyModel model, Delegate delegate, Profile profile) {
        mContext = context;
        mModel = model;
        mDelegate = delegate;
        mProfile = profile;
    }

    boolean isInLandscapeMode() {
        return mContext.getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE;
    }

    void showSheetForPix(List<BankAccount> bankAccounts) {
        mInputProtector.markShowTime();
        if (bankAccounts == null || bankAccounts.isEmpty()) {
            return;
        }

        mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
        mModel.set(SCREEN, FOP_SELECTOR);
        ModelList screenItems = mModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        screenItems.clear();

        for (BankAccount bankAccount : bankAccounts) {
            final PropertyModel model = createBankAccountModel(mContext, bankAccount);
            screenItems.add(new ListItem(BANK_ACCOUNT, model));
        }

        screenItems.add(buildPixAdditionalInfo());

        maybeShowContinueButton(screenItems, Set.of(BANK_ACCOUNT));

        screenItems.add(0, buildPixHeader(mContext));
        screenItems.add(buildPixFooter());

        mModel.set(SURVIVES_NAVIGATION, false);
        mModel.set(VISIBLE_STATE, SHOWN);
        mInputProtector.markShowTime();
    }

    void showSheetForPaymentLink(List<Ewallet> ewallets, List<ResolveInfo> apps) {
        mInputProtector.markShowTime();
        boolean isEwalletAvailable = ewallets != null && !ewallets.isEmpty();
        boolean isPaymentAppAvailable =
                apps != null
                        && !apps.isEmpty()
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT);
        if (!isEwalletAvailable && !isPaymentAppAvailable) {
            return;
        }

        mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
        mModel.set(SCREEN, FOP_SELECTOR);
        ModelList screenItems = mModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        screenItems.clear();

        for (Ewallet ewallet : ewallets) {
            PropertyModel model = createEwalletModel(mContext, ewallet);
            screenItems.add(new ListItem(EWALLET, model));
        }

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT)) {
            for (ResolveInfo app : apps) {
                PropertyModel model = createPaymentAppModel(mContext, app);
                screenItems.add(new ListItem(PAYMENT_APP, model));
            }
        }
        screenItems.add(buildPaymentLinkAdditionalInfo(ewallets, apps));

        maybeShowContinueButton(screenItems, Set.of(EWALLET, PAYMENT_APP));

        screenItems.add(0, buildPaymentLinkHeader(mContext, ewallets, apps));
        screenItems.add(buildPaymentLinkFooter(ewallets, apps));

        mModel.set(SURVIVES_NAVIGATION, false);
        mModel.set(VISIBLE_STATE, SHOWN);
        mInputProtector.markShowTime();
    }

    void showProgressScreen() {
        // The {@link VISIBLE_STATE} of {@link SHOWN} has 2 functions:
        // 1. If the bottom sheet is not open, i.e. {@code VISIBLE_STATE = HIDDEN}, setting {@code
        // VISIBLE_STATE = SHOWN} opens and shows a new screen.
        // 2. If the bottom sheet is already open and showing a screen, i.e. {@code VISIBLE_STATE =
        // SHOWN}, setting it again to {@code VISIBLE_STATE = SHOWN} swaps the existing screen to
        // show a new screen.
        // Since a property can't be assigned to the value it already has, a placeholder state
        // {@link SWAPPING_SCREEN} is introduced to facilitate setting {@code VISIBLE_STATE = SHOWN}
        // again.
        mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
        mModel.set(SCREEN, PROGRESS_SCREEN);
        mModel.set(SURVIVES_NAVIGATION, false);
        mModel.set(VISIBLE_STATE, SHOWN);
    }

    void showErrorScreen() {
        // Set {@link VISIBLE_STATE} to the placeholder state which is a no-op, and then update the
        // screen to the error screen.
        mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
        mModel.set(SCREEN, ERROR_SCREEN);
        // Set error screen properties and show the screen.
        mModel.get(SCREEN_VIEW_MODEL).set(PRIMARY_BUTTON_CALLBACK, v -> dismiss());
        mModel.set(SURVIVES_NAVIGATION, false);
        mModel.set(VISIBLE_STATE, SHOWN);
    }

    void dismiss() {
        mModel.set(SCREEN, UNINITIALIZED);
        mModel.set(VISIBLE_STATE, HIDDEN);
    }

    public void onUiEvent(@UiEvent int uiEvent) {
        mDelegate.onUiEvent(uiEvent);
    }

    void showPixAccountLinkingPrompt() {
        // Set {@link VISIBLE_STATE} to the placeholder state which is a no-op, and then update the
        // screen to the Pix account linking prompt. Finally update {@link VISIBLE_STATE} to show
        // the new screen.
        mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
        mModel.set(SCREEN, PIX_ACCOUNT_LINKING_PROMPT);
        // Set Pix account linking prompt properties and show the prompt.
        mModel.get(SCREEN_VIEW_MODEL)
                .set(ACCEPT_BUTTON_CALLBACK, v -> mDelegate.onPixAccountLinkingPromptAccepted());
        mModel.get(SCREEN_VIEW_MODEL)
                .set(DECLINE_BUTTON_CALLBACK, v -> mDelegate.onPixAccountLinkingPromptDeclined());
        // Prevent the bottom sheet from closing during page navigations.
        mModel.set(SURVIVES_NAVIGATION, true);
        mModel.set(VISIBLE_STATE, SHOWN);
    }

    @VisibleForTesting
    ListItem buildPixHeader(Context context) {
        String title =
                context.getString(
                        R.string.facilitated_payments_payment_methods_bottom_sheet_detailed_title,
                        context.getString(R.string.settings_manage_other_financial_accounts_pix));

        int productIconHeight =
                (int)
                        context.getResources()
                                .getDimension(R.dimen.facilitated_payments_product_icon_height);

        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(DESCRIPTION_ID, R.string.pix_payment_methods_bottom_sheet_description)
                        .with(PRODUCT_ICON_DRAWABLE_ID, R.drawable.gpay_pix_logo)
                        .with(PRODUCT_ICON_HEIGHT, productIconHeight)
                        .with(
                                PRODUCT_ICON_CONTENT_DESCRIPTION_ID,
                                R.string.pix_payment_product_icon_content_description)
                        .with(TITLE, title)
                        .build());
    }

    @VisibleForTesting
    ListItem buildPaymentLinkHeader(
            Context context, List<Ewallet> ewallets, List<ResolveInfo> apps) {
        PropertyModel.Builder headerBuilder =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(PRODUCT_ICON_DRAWABLE_ID, 0);
        if (ewallets != null && !ewallets.isEmpty()) {
            int productIconHeight =
                    (int)
                            context.getResources()
                                    .getDimension(
                                            R.dimen.facilitated_payments_gpay_icon_header_height);
            headerBuilder
                    .with(PRODUCT_ICON_DRAWABLE_ID, R.drawable.google_pay)
                    .with(PRODUCT_ICON_HEIGHT, productIconHeight)
                    .with(
                            PRODUCT_ICON_CONTENT_DESCRIPTION_ID,
                            R.string.facilitated_payments_google_pay);
        }
        if (apps != null && !apps.isEmpty()) {
            headerBuilder.with(
                    PAYMENT_LINK_TITLE_TOP_MARGIN,
                    (int)
                            context.getResources()
                                    .getDimension(
                                            R.dimen.facilitated_payments_fop_title_top_margin));
        }
        headerBuilder.with(TITLE, getPaymentLinkHeaderTitle(context, ewallets, apps));

        if (ewallets.size() == 1 && !ewallets.get(0).getIsFidoEnrolled() && apps.isEmpty()) {
            headerBuilder.with(SECURITY_CHECK_DRAWABLE_ID, R.drawable.security_check_illustration);
            headerBuilder.with(
                    DESCRIPTION_ID,
                    R.string.ewallet_first_time_check_payment_methods_bottom_sheet_description);
        }

        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER, headerBuilder.build());
    }

    // This method will always return a valid title, there should not be any case where eWallet and
    // payment app both are not present.
    private String getPaymentLinkHeaderTitle(
            Context context, List<Ewallet> ewallets, List<ResolveInfo> apps) {
        if (apps != null
                && !apps.isEmpty()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT)) {
            return context.getString(
                    R.string.facilitated_payments_non_card_payment_methods_bottom_sheet_title);
        }

        assert ewallets != null && !ewallets.isEmpty()
                : "At least a single eWallet must be present.";

        // This will contain the shared ewallet name if all eWallets have the same name;
        // otherwise, it will contain `null`.
        String sharedEwalletName = ewallets.get(0).getEwalletName();
        for (Ewallet ewallet : ewallets) {
            if (!sharedEwalletName.equals(ewallet.getEwalletName())) {
                sharedEwalletName = null;
                break;
            }
        }
        if (sharedEwalletName != null) {
            // If all ewallets have same name, return a specific title containing that eWallet name.
            return context.getString(
                    R.string.facilitated_payments_payment_methods_bottom_sheet_detailed_title,
                    sharedEwalletName);
        }
        // If ewallets have different names, return a generic title.
        return context.getString(
                R.string.facilitated_payments_payment_methods_bottom_sheet_generic_title);
    }

    private ListItem buildPixFooter() {
        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                                () ->
                                        this.onManagePaymentMethodsOptionSelected(
                                                PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM))
                        .build());
    }

    private ListItem buildPaymentLinkFooter(List<Ewallet> ewallets, List<ResolveInfo> apps) {
        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(
                                FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                                () -> {
                                    startSettings(PAYMENT_METHODS);
                                    if (!ewallets.isEmpty()) {
                                        this.recordManagePaymentMethodsOptionSelected(
                                                getEwalletFopSelectorUserActionHistogram(ewallets));
                                    }
                                    if (ChromeFeatureList.isEnabled(
                                            ChromeFeatureList
                                                    .FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT)) {
                                        this
                                                .recordManagePaymentMethodsOptionSelectedNonCardPaymentMethods(
                                                        getPaymentLinkFopSelectorTypesUserActionHistogram(
                                                                !ewallets.isEmpty(),
                                                                !apps.isEmpty()));
                                    }
                                })
                        .build());
    }

    @VisibleForTesting
    ListItem buildPixAdditionalInfo() {
        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO,
                new PropertyModel.Builder(AdditionalInfoProperties.ALL_KEYS)
                        .with(
                                AdditionalInfoProperties.DESCRIPTION_ID,
                                R.string.pix_payment_additional_info)
                        .with(
                                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                                () -> {
                                    startSettings(FINANCIAL_ACCOUNTS);
                                    recordHistogramOnTurnOffPaymentPromptLinkClicked(
                                            PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM);
                                })
                        .build());
    }

    @VisibleForTesting
    ListItem buildPaymentLinkAdditionalInfo(List<Ewallet> ewallets, List<ResolveInfo> apps) {
        PropertyModel.Builder propertyModelBuilder =
                new PropertyModel.Builder(AdditionalInfoProperties.ALL_KEYS);

        int descriptionId = getPaymentAdditionalInfoString(!ewallets.isEmpty(), !apps.isEmpty());
        if (descriptionId != 0) {
            propertyModelBuilder.with(AdditionalInfoProperties.DESCRIPTION_ID, descriptionId);
        }

        propertyModelBuilder.with(
                SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK,
                () -> {
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM)) {
                        startSettings(NON_CARD_PAYMENT_METHODS);
                        recordHistogramOnTurnOffPaymentPromptLinkNonCardPaymentMethodsClicked(
                                getPaymentLinkFopSelectorTypesUserActionHistogram(
                                        !ewallets.isEmpty(), !apps.isEmpty()));
                    } else {
                        startSettings(FINANCIAL_ACCOUNTS);
                    }
                    if (!ewallets.isEmpty()) {
                        recordHistogramOnTurnOffPaymentPromptLinkClicked(
                                getEwalletFopSelectorUserActionHistogram(ewallets));
                    }
                });

        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO,
                propertyModelBuilder.build());
    }

    private void startSettings(int settingsFragment) {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mContext, settingsFragment);
    }

    @VisibleForTesting
    PropertyModel createBankAccountModel(Context context, BankAccount bankAccount) {
        return new PropertyModel.Builder(BankAccountProperties.NON_TRANSFORMING_KEYS)
                .with(BANK_NAME, bankAccount.getBankName())
                .with(
                        BANK_ACCOUNT_PAYMENT_RAIL,
                        context.getString(R.string.settings_manage_other_financial_accounts_pix)
                                + "  •  ")
                .with(
                        BANK_ACCOUNT_TYPE,
                        getBankAccountTypeString(context, bankAccount.getAccountType()))
                .with(BANK_ACCOUNT_NUMBER, bankAccount.getObfuscatedAccountNumber())
                .with(BANK_ACCOUNT_TRANSACTION_LIMIT, getBankAccountTransactionLimit(context))
                .with(ON_BANK_ACCOUNT_CLICK_ACTION, () -> this.onBankAccountSelected(bankAccount))
                .with(
                        BANK_ACCOUNT_ICON,
                        AutofillImageFetcherFactory.getForProfile(mProfile)
                                .getPixAccountIcon(context, bankAccount.getDisplayIconUrl()))
                .build();
    }

    @VisibleForTesting
    PropertyModel createEwalletModel(Context context, Ewallet ewallet) {
        PropertyModel.Builder ewalletModelBuilder =
                new PropertyModel.Builder(EwalletProperties.NON_TRANSFORMING_KEYS)
                        .with(EWALLET_NAME, ewallet.getEwalletName())
                        .with(ACCOUNT_DISPLAY_NAME, ewallet.getAccountDisplayName())
                        .with(ON_EWALLET_CLICK_ACTION, () -> this.onEwalletSelected(ewallet));
        Bitmap ewalletIcon = null;
        if (ewallet.getDisplayIconUrl() != null && ewallet.getDisplayIconUrl().isValid()) {
            ewalletIcon =
                    AutofillImageFetcherFactory.getForProfile(mProfile)
                            .getImageIfAvailable(
                                    ewallet.getDisplayIconUrl(),
                                    IconSpecs.create(
                                            context,
                                            ImageType.CREDIT_CARD_ART_IMAGE,
                                            ImageSize.LARGE));
        }
        if (ewalletIcon != null) {
            ewalletModelBuilder.with(EWALLET_ICON_BITMAP, ewalletIcon);
        } else {
            ewalletModelBuilder.with(EWALLET_DRAWABLE_ID, R.drawable.ic_account_balance);
        }
        return ewalletModelBuilder.build();
    }

    @VisibleForTesting
    PropertyModel createPaymentAppModel(Context context, ResolveInfo app) {
        PackageManager packageManager = context.getPackageManager();
        return new PropertyModel.Builder(PaymentAppProperties.NON_TRANSFORMING_KEYS)
                .with(PAYMENT_APP_NAME, app.loadLabel(packageManager).toString())
                .with(PAYMENT_APP_ICON, app.loadIcon(packageManager))
                .with(ON_PAYMENT_APP_CLICK_ACTION, () -> this.onPaymentAppSelected(app))
                .build();
    }

    public void onBankAccountSelected(BankAccount bankAccount) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.onBankAccountSelected(bankAccount.getInstrumentId());
    }

    public void onEwalletSelected(Ewallet ewallet) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.onEwalletSelected(ewallet.getInstrumentId());
    }

    public void onPaymentAppSelected(ResolveInfo app) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.onPaymentAppSelected(app.activityInfo.packageName, app.activityInfo.name);
    }

    private static int getPaymentAdditionalInfoString(boolean hasEwallets, boolean hasApps) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT)) {
            if (hasApps && hasEwallets) {
                return R.string.ewallet_and_payment_app_payment_additional_info;
            }
            if (hasApps) {
                return R.string.payment_app_payment_additional_info;
            }
        }
        return hasEwallets ? R.string.ewallet_payment_additional_info : 0;
    }

    private void onManagePaymentMethodsOptionSelected(String histogramName) {
        startSettings(PAYMENT_METHODS);

        recordManagePaymentMethodsOptionSelected(histogramName);
    }

    private void recordManagePaymentMethodsOptionSelected(String histogramName) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName,
                FopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED,
                FopSelectorAction.MAX_VALUE);
    }

    private void recordManagePaymentMethodsOptionSelectedNonCardPaymentMethods(
            String histogramName) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName,
                PaymentLinkFopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED,
                PaymentLinkFopSelectorAction.MAX_VALUE);
    }

    private void recordHistogramOnTurnOffPaymentPromptLinkClicked(String histogramName) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName,
                FopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED,
                FopSelectorAction.MAX_VALUE);
    }

    private void recordHistogramOnTurnOffPaymentPromptLinkNonCardPaymentMethodsClicked(
            String histogramName) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName,
                PaymentLinkFopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED,
                PaymentLinkFopSelectorAction.MAX_VALUE);
    }

    private String getEwalletFopSelectorUserActionHistogram(List<Ewallet> ewallets) {
        if (ewallets.size() == 1) {
            if (ewallets.get(0).getIsFidoEnrolled()) {
                return EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM + "SingleBoundEwallet";
            }
            return EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM + "SingleUnboundEwallet";
        }
        return EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM + "MultipleEwallets";
    }

    static String getBankAccountTransactionLimit(Context context) {
        return context.getString(
                R.string.pix_bank_account_transaction_limit, PIX_BANK_ACCOUNT_TRANSACTION_LIMIT);
    }

    @VisibleForTesting
    static String getBankAccountTypeString(Context context, @AccountType int bankAccountType) {
        switch (bankAccountType) {
            case AccountType.CHECKING:
                return context.getString(R.string.bank_account_type_checking);
            case AccountType.SAVINGS:
                return context.getString(R.string.bank_account_type_savings);
            case AccountType.CURRENT:
                return context.getString(R.string.bank_account_type_current);
            case AccountType.SALARY:
                return context.getString(R.string.bank_account_type_salary);
            case AccountType.TRANSACTING_ACCOUNT:
                return context.getString(R.string.bank_account_type_transacting);
            case AccountType.UNKNOWN:
            default:
                return "";
        }
    }

    private static String getPaymentLinkFopSelectorTypesUserActionHistogram(
            boolean hasEwallet, boolean hasApps) {
        if (hasApps && hasEwallet) {
            return PAYMENT_LINK_FOP_SELECTOR_TYPES_USER_ACTION_HISTOGRAM.replace(
                    "{PaymentLinkFopSelectorTypes}", "EwalletAndA2A");
        }
        return hasApps
                ? PAYMENT_LINK_FOP_SELECTOR_TYPES_USER_ACTION_HISTOGRAM.replace(
                        "{PaymentLinkFopSelectorTypes}", "A2AOnly")
                : PAYMENT_LINK_FOP_SELECTOR_TYPES_USER_ACTION_HISTOGRAM.replace(
                        "{PaymentLinkFopSelectorTypes}", "EwalletOnly");
    }

    // Continue button is shown when among all the targetTypes only one targetType is present. The
    // Single targetType present should also have only one occurrence in the screenItems.
    private static void maybeShowContinueButton(ModelList screenItems, Set<Integer> targetTypes) {
        Map<Integer, ArrayList<ListItem>> listItemsMap = new HashMap<>();

        for (ListItem item : screenItems) {
            if (targetTypes.contains(item.type)) {
                if (listItemsMap.containsKey(item.type)) {
                    listItemsMap.get(item.type).add(item);
                } else {
                    listItemsMap.put(item.type, new ArrayList<>(List.of(item)));
                }
            }
        }
        if (listItemsMap.size() == 1) {
            ArrayList<ListItem> allListItemsOfTargetType =
                    listItemsMap.get(listItemsMap.keySet().iterator().next());
            if (allListItemsOfTargetType != null && allListItemsOfTargetType.size() == 1) {
                screenItems.add(
                        new ListItem(CONTINUE_BUTTON, allListItemsOfTargetType.get(0).model));
            }
        }
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}
