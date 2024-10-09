// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.hardware.biometrics.BiometricManager;
import android.os.Build;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.util.Pair;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.hardware.fingerprint.FingerprintManagerCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.MandatoryReauthAuthenticationFlowEvent;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.payments.AndroidPaymentAppFactory;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * Autofill credit cards fragment, which allows the user to edit credit cards and control payment
 * apps.
 */
public class AutofillPaymentMethodsFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver {
    // The Fido pref is used as a key on the settings toggle. This key helps in the retrieval of the
    // Fido toggle during tests.
    static final String PREF_FIDO = "fido";
    static final String PREF_DELETE_SAVED_CVCS = "delete_saved_cvcs";
    static final String PREF_MANDATORY_REAUTH = "mandatory_reauth";
    static final String PREF_SAVE_CVC = "save_cvc";
    static final String PREF_ADD_IBAN = "add_iban";
    static final String PREF_IBAN = "iban";
    static final String PREF_CARD_BENEFITS = "card_benefits";
    private static final String PREF_PAYMENT_APPS = "payment_apps";

    @VisibleForTesting
    static final String PREF_FINANCIAL_ACCOUNTS_MANAGEMENT = "financial_accounts_management";

    private static final String AUTOFILL_MANAGE_PAYMENTS_URL =
            "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    private static final String AUTOFILL_MANAGE_PAYMENTS_SANDBOX_URL =
            "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";

    static final String MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.AuthEvent.SettingsPage.EditCard";
    static final String MANDATORY_REAUTH_OPT_IN_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage.OptIn";
    static final String MANDATORY_REAUTH_OPT_OUT_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage.OptOut";

    @Nullable private ReauthenticatorBridge mReauthenticatorBridge;
    @Nullable private AutofillPaymentMethodsDelegate mAutofillPaymentMethodsDelegate;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private Callback<String> mServerIbanManageLinkOpenerCallback =
            url -> CustomTabActivity.showInfoPage(getActivity(), url);

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.autofill_payment_methods));
        setHasOptionsMenu(true);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            getHelpAndFeedbackLauncher()
                    .show(
                            getActivity(),
                            getActivity().getString(R.string.help_context_autofill),
                            null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of credit cards.  Although we could detect if credit cards are
        // added or deleted, the credit card summary (number) might be different.  To be safe, we
        // update all.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        ChromeSwitchPreference autofillSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_credit_cards_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_credit_cards_toggle_sublabel);
        autofillSwitch.setChecked(personalDataManager.isAutofillPaymentMethodsEnabled());
        autofillSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    personalDataManager.setAutofillCreditCardEnabled((boolean) newValue);
                    return true;
                });
        autofillSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return personalDataManager.isAutofillCreditCardManaged();
                    }

                    @Override
                    public boolean isPreferenceClickDisabled(Preference preference) {
                        return personalDataManager.isAutofillCreditCardManaged()
                                && !personalDataManager.isAutofillPaymentMethodsEnabled();
                    }
                });
        getPreferenceScreen().addPreference(autofillSwitch);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS)) {
            Pair<Integer, String> otherFinancialAccountTypes =
                    getOtherFinancialAccountsTypes(personalDataManager);
            if (otherFinancialAccountTypes.first != 0) {
                Preference otherFinancialAccountsPref = new Preference(getStyledContext());
                otherFinancialAccountsPref.setKey(PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
                otherFinancialAccountsPref.setSingleLineTitle(false);
                otherFinancialAccountsPref.setTitle(
                        getResources()
                                .getString(
                                        R.string.settings_manage_other_financial_accounts_title,
                                        otherFinancialAccountTypes.second));
                otherFinancialAccountsPref.setSummary(
                        getResources()
                                .getQuantityString(
                                        R.plurals
                                                .settings_manage_other_financial_accounts_description,
                                        otherFinancialAccountTypes.first,
                                        otherFinancialAccountTypes.second));
                getPreferenceScreen().addPreference(otherFinancialAccountsPref);
                otherFinancialAccountsPref.setOnPreferenceClickListener(
                        this::showOtherFinancialAccountsFragment);
            }
        }

        // TODO(crbug.com/40261690): Confirm with Product on the order of the toggles.
        // Don't show the toggle to enable mandatory reauth on automotive,
        // as the feature is always enabled for automotive builds.
        if (BuildInfo.getInstance().isAutomotive) {
            // The ReauthenticatorBridge is still needed for reauthentication to view/edit
            // payment methods.
            createReauthenticatorBridge();
        } else {
            createReauthenticatorBridge();
            createMandatoryReauthSwitch();
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE)) {
            ChromeSwitchPreference saveCvcSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            saveCvcSwitch.setTitle(R.string.autofill_settings_page_enable_cvc_storage_label);
            saveCvcSwitch.setSummary(R.string.autofill_settings_page_enable_cvc_storage_sublabel);
            saveCvcSwitch.setKey(PREF_SAVE_CVC);
            // When "Save And Fill Payments Methods" is disabled, we disable this cvc storage
            // toggle.
            saveCvcSwitch.setEnabled(personalDataManager.isAutofillPaymentMethodsEnabled());
            saveCvcSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        personalDataManager.setAutofillPaymentCvcStorage((boolean) newValue);
                        return true;
                    });
            getPreferenceScreen().addPreference(saveCvcSwitch);

            // When "Save And Fill Payments Methods" is disabled, we override this toggle's value to
            // off (but not change the underlying pref value). When "Save And Fill Payments Methods"
            // is ON, show the cvc storage pref value.
            saveCvcSwitch.setChecked(
                    personalDataManager.isAutofillPaymentMethodsEnabled()
                            && personalDataManager.isPaymentCvcStorageEnabled());

            // Add the deletion button for saved CVCs. Note that this button's presence doesn't
            // depend on the value of the "Save and fill payment methods" toggle, since we would
            // like to allow the user to delete saved CVCs even when the toggle is disabled.
            // Conditionally show the deletion button based on whether there are any CVCs stored.
            if (personalDataManager.getCreditCardsForSettings().stream()
                    .anyMatch(card -> !card.getCvc().isEmpty())) {
                createDeleteSavedCvcsButton();
            }
        }

        if (personalDataManager.isAutofillPaymentMethodsEnabled()
                && (ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS)
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE))) {
            Preference cardBenefitsPref = new Preference(getStyledContext());
            cardBenefitsPref.setTitle(R.string.autofill_settings_page_card_benefits_label);
            cardBenefitsPref.setSummary(
                    R.string.autofill_settings_page_card_benefits_preference_summary);
            cardBenefitsPref.setKey(PREF_CARD_BENEFITS);
            cardBenefitsPref.setFragment(AutofillCardBenefitsFragment.class.getName());
            getPreferenceScreen().addPreference(cardBenefitsPref);
        }

        for (CreditCard card : personalDataManager.getCreditCardsForSettings()) {
            // Add a preference for the credit card.
            Preference card_pref = new Preference(getStyledContext());
            // Make the card_pref multi-line, since cards with long nicknames won't fit on a single
            // line.
            card_pref.setSingleLineTitle(false);
            card_pref.setTitle(card.getCardLabel());

            // Show virtual card enabled status for enrolled cards, expiration date otherwise.
            if (card.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED
                    && ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)) {
                card_pref.setSummary(R.string.autofill_virtual_card_enrolled_text);
            } else {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE)
                        && !card.getCvc().isEmpty()) {
                    card_pref.setSummary(
                            card.getFormattedExpirationDateWithCvcSavedMessage(getActivity()));
                } else {
                    card_pref.setSummary(card.getFormattedExpirationDate(getActivity()));
                }
            }

            // Set card icon. It can be either a custom card art or a network icon.
            card_pref.setIcon(
                    getCardIcon(
                            getStyledContext(),
                            personalDataManager,
                            card.getCardArtUrl(),
                            card.getIssuerIconDrawableId(),
                            ImageSize.LARGE,
                            ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)));

            if (card.getIsLocal()) {
                card_pref.setOnPreferenceClickListener(
                        this::showLocalCardEditPageAfterAuthenticationIfRequired);
            } else {
                card_pref.setFragment(AutofillServerCardEditor.class.getName());
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)) {
                    card_pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
                } else {
                    card_pref.setWidgetLayoutResource(R.layout.autofill_server_data_text_label);
                }
            }

            Bundle args = card_pref.getExtras();
            args.putString(AutofillEditorBase.AUTOFILL_GUID, card.getGUID());
            getPreferenceScreen().addPreference(card_pref);
        }

        boolean showLocalIbans =
                ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN);
        boolean showServerIbans =
                ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN);

        // Display all IBANs.
        for (Iban iban : personalDataManager.getIbansForSettings()) {
            if ((iban.getRecordType() == IbanRecordType.LOCAL_IBAN && !showLocalIbans)
                    || (iban.getRecordType() == IbanRecordType.SERVER_IBAN && !showServerIbans)) {
                continue;
            }
            Preference iban_pref = new Preference(getStyledContext());
            iban_pref.setIcon(R.drawable.iban_icon);
            iban_pref.setSingleLineTitle(false);
            iban_pref.setTitle(iban.getLabel());
            iban_pref.setSummary(iban.getNickname());
            if (iban.getRecordType() == IbanRecordType.LOCAL_IBAN) {
                iban_pref.setFragment(AutofillLocalIbanEditor.class.getName());
                Bundle args = iban_pref.getExtras();
                args.putString(AutofillEditorBase.AUTOFILL_GUID, iban.getGuid());
            } else if (iban.getRecordType() == IbanRecordType.SERVER_IBAN) {
                iban_pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
                iban_pref.setOnPreferenceClickListener(
                        preference -> {
                            mServerIbanManageLinkOpenerCallback.onResult(getManageIbanLink(iban));
                            return true;
                        });
            }
            getPreferenceScreen().addPreference(iban_pref);
            iban_pref.setKey(PREF_IBAN);
        }

        // Add 'Add credit card' button. Tap of it brings up card editor which allows users type in
        // new credit cards.
        if (personalDataManager.isAutofillPaymentMethodsEnabled()) {
            Preference add_card_pref = new Preference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(
                    SemanticColorUtils.getDefaultControlColorActive(getContext()),
                    PorterDuff.Mode.SRC_IN);
            add_card_pref.setIcon(plusIcon);
            add_card_pref.setTitle(R.string.autofill_create_credit_card);
            add_card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            getPreferenceScreen().addPreference(add_card_pref);
        }

        // Add 'Add IBAN' button. Tapping it brings up the IBAN editor which allows users to type in
        // a new IBAN.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN)
                && personalDataManager.isAutofillPaymentMethodsEnabled()
                && personalDataManager.shouldShowAddIbanButtonOnSettingsPage()) {
            Preference add_iban_pref = new Preference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(
                    SemanticColorUtils.getDefaultControlColorActive(getContext()),
                    PorterDuff.Mode.SRC_IN);
            add_iban_pref.setIcon(plusIcon);
            add_iban_pref.setTitle(R.string.autofill_add_local_iban);
            add_iban_pref.setKey(PREF_ADD_IBAN);
            add_iban_pref.setFragment(AutofillLocalIbanEditor.class.getName());
            getPreferenceScreen().addPreference(add_iban_pref);
        }

        // Add the link to payment apps only after the credit card list is rebuilt.
        Preference payment_apps_pref = new Preference(getStyledContext());
        payment_apps_pref.setTitle(R.string.payment_apps_title);
        payment_apps_pref.setFragment(AndroidPaymentAppsFragment.class.getCanonicalName());
        payment_apps_pref.setShouldDisableView(true);
        payment_apps_pref.setKey(PREF_PAYMENT_APPS);
        getPreferenceScreen().addPreference(payment_apps_pref);
        refreshPaymentAppsPrefForAndroidPaymentApps(payment_apps_pref);
    }

    private void createReauthenticatorBridge() {
        if (mReauthenticatorBridge == null) {
            mReauthenticatorBridge =
                    ReauthenticatorBridge.create(
                            this.getActivity(), getProfile(), DeviceAuthSource.AUTOFILL);
        }
    }

    private void createMandatoryReauthSwitch() {
        ChromeSwitchPreference mandatoryReauthSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        mandatoryReauthSwitch.setTitle(
                R.string.autofill_settings_page_enable_payment_method_mandatory_reauth_label);
        mandatoryReauthSwitch.setSummary(
                R.string.autofill_settings_page_enable_payment_method_mandatory_reauth_sublabel);
        mandatoryReauthSwitch.setKey(PREF_MANDATORY_REAUTH);
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        // We always display the toggle, but the toggle is only enabled when Autofill credit
        // card is enabled AND the device supports biometric auth or screen lock. If either of
        // these is not met, we will grey out the toggle.
        // `getBiometricAvailabilityStatus` also checks if screen lock is available and returns
        // `ONLY_LSKF_AVAILABLE` if it is.
        boolean enableReauthSwitch =
                personalDataManager.isAutofillPaymentMethodsEnabled()
                        && (mReauthenticatorBridge.getBiometricAvailabilityStatus()
                                != BiometricStatus.UNAVAILABLE);
        mandatoryReauthSwitch.setEnabled(enableReauthSwitch);
        mandatoryReauthSwitch.setOnPreferenceChangeListener(this::onMandatoryReauthSwitchToggled);
        getPreferenceScreen().addPreference(mandatoryReauthSwitch);

        // Every {@link SwitchPreferenceCompat} on a {@link PreferenceScreen} has a pref that is
        // automatically added to the {@link SharedPreferences}. When a switch is added, by
        // default its checked state is reset to the saved pref value irrespective of whether or
        // not the switch's checked state was set before adding the switch. Setting the checked
        // state after adding the switch updates the underlying pref as well.
        // If a user opts in to mandatory reauth during the checkout flow, since the switch's
        // underlying pref is still false, the switch does not reflect the opt-in. Set switch's
        // checked state after adding it to the screen so the underlying pref value is also
        // updated and is in sync with the mandatory reauth user pref.
        mandatoryReauthSwitch.setChecked(
                personalDataManager.isPaymentMethodsMandatoryReauthEnabled());
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void refreshPaymentAppsPrefForAndroidPaymentApps(Preference pref) {
        if (AndroidPaymentAppFactory.hasAndroidPaymentApps()) {
            setPaymentAppsPrefStatus(pref, true);
        } else {
            refreshPaymentAppsPrefForServiceWorkerPaymentApps(pref);
        }
    }

    private void refreshPaymentAppsPrefForServiceWorkerPaymentApps(Preference pref) {
        ServiceWorkerPaymentAppBridge.hasServiceWorkerPaymentApps(
                new ServiceWorkerPaymentAppBridge.HasServiceWorkerPaymentAppsCallback() {
                    @Override
                    public void onHasServiceWorkerPaymentAppsResponse(boolean hasPaymentApps) {
                        setPaymentAppsPrefStatus(pref, hasPaymentApps);
                    }
                });
    }

    private void setPaymentAppsPrefStatus(Preference pref, boolean enabled) {
        if (enabled) {
            pref.setSummary(null);
            pref.setEnabled(true);
        } else {
            pref.setSummary(R.string.payment_no_apps_summary);
            pref.setEnabled(false);
        }
    }

    private boolean isBiometricAvailable() {
        // Only Android versions 9 and above are supported.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            BiometricManager biometricManager =
                    getStyledContext().getSystemService(BiometricManager.class);
            return biometricManager != null
                    && biometricManager.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS;
        } else {
            // For API level < Q, we will use FingerprintManagerCompat to check enrolled
            // fingerprints. Note that for API level lower than 23, FingerprintManagerCompat behaves
            // like no fingerprint hardware and no enrolled fingerprints.
            FingerprintManagerCompat fingerprintManager =
                    FingerprintManagerCompat.from(getStyledContext());
            return fingerprintManager != null
                    && fingerprintManager.isHardwareDetected()
                    && fingerprintManager.hasEnrolledFingerprints();
        }
    }

    /** Handle preference changes from mandatory reauth toggle */
    private boolean onMandatoryReauthSwitchToggled(Preference preference, Object newValue) {
        assert preference.getKey().equals(PREF_MANDATORY_REAUTH);

        ChromeSwitchPreference mandatoryReauthSwitch = (ChromeSwitchPreference) preference;
        // If the user preference update is successful, toggle the switch to the success state.
        boolean userIntendedState = !mandatoryReauthSwitch.isChecked();
        String histogramName =
                userIntendedState
                        ? MANDATORY_REAUTH_OPT_IN_HISTOGRAM
                        : MANDATORY_REAUTH_OPT_OUT_HISTOGRAM;
        RecordHistogram.recordEnumeratedHistogram(
                histogramName,
                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
        // We require user authentication every time user tries to change this
        // preference. Set useLastValidAuth=false to skip the grace period.
        mReauthenticatorBridge.reauthenticate(
                success -> {
                    if (success) {
                        // Only set the preference to new value when user passes the
                        // authentication.
                        PersonalDataManagerFactory.getForProfile(getProfile())
                                .setAutofillPaymentMethodsMandatoryReauth((boolean) newValue);

                        // When the preference is updated, the page is expected to refresh and show
                        // the updated preference. Fallback if the page does not load.
                        mandatoryReauthSwitch.setChecked(userIntendedState);
                        RecordHistogram.recordEnumeratedHistogram(
                                histogramName,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED,
                                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
                    } else {
                        RecordHistogram.recordEnumeratedHistogram(
                                histogramName,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_FAILED,
                                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
                    }
                });
        // Returning false here holds the toggle to still display the old value while
        // waiting for biometric auth. Once biometric is completed (either succeed or
        // fail), OnResume will reload the page with the pref value, which will switch
        // to the new value if biometric auth succeeded.
        return false;
    }

    /**
     * If mandatory reauth is enabled, trigger device authentication before user can view/edit local
     * card. Else show the local card edit page.
     *
     * @param preference The {@link Preference} for the local card.
     * @return true if the click was handled, false otherwise.
     */
    private boolean showLocalCardEditPageAfterAuthenticationIfRequired(Preference preference) {
        if (!PersonalDataManagerFactory.getForProfile(getProfile())
                .isPaymentMethodsMandatoryReauthEnabled()) {
            showLocalCardEditPage(preference);
            return true;
        }

        // mReauthenticatorBridge should be initiated already when determining whether to show the
        // mandatory reauth toggle.
        assert mReauthenticatorBridge != null;
        RecordHistogram.recordEnumeratedHistogram(
                MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
        // When mandatory reauth is enabled, offer device authentication challenge.
        mReauthenticatorBridge.reauthenticate(
                success -> {
                    // If authentication is successful, manually trigger the local card edit page.
                    // Else, stay on this page.
                    if (success) {
                        RecordHistogram.recordEnumeratedHistogram(
                                MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED,
                                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
                        showLocalCardEditPage(preference);
                    } else {
                        RecordHistogram.recordEnumeratedHistogram(
                                MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_FAILED,
                                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
                    }
                });
        return true;
    }

    /**
     * Show the local card edit page for the given local card.
     *
     * @param preference The {@link Preference} for the local card.
     */
    private void showLocalCardEditPage(Preference preference) {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                getActivity(), AutofillLocalCardEditor.class, preference.getExtras());
    }

    /**
     * Create a clickable "Delete saved cvcs" button and add it to the preference screen.
     * No divider line above this preference.
     */
    private void createDeleteSavedCvcsButton() {
        ChromeBasePreference deleteSavedCvcs = new ChromeBasePreference(getStyledContext());
        deleteSavedCvcs.setKey(PREF_DELETE_SAVED_CVCS);
        SpannableString spannableString =
                new SpannableString(
                        getResources()
                                .getString(R.string.autofill_settings_page_bulk_remove_cvc_label));
        spannableString.setSpan(
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorLink(getContext())),
                0,
                spannableString.length(),
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        deleteSavedCvcs.setSummary(spannableString);
        deleteSavedCvcs.setDividerAllowedAbove(false);
        deleteSavedCvcs.setOnPreferenceClickListener(
                preference -> {
                    showDeleteSavedCvcsConfirmationDialog();
                    return true;
                });
        getPreferenceScreen().addPreference(deleteSavedCvcs);
    }

    private void showDeleteSavedCvcsConfirmationDialog() {
        AutofillDeleteSavedCvcsConfirmationDialog dialog =
                new AutofillDeleteSavedCvcsConfirmationDialog(
                        getActivity(),
                        new ModalDialogManager(
                                new AppModalPresenter(getActivity()), ModalDialogType.APP),
                        deleteRequested -> {
                            if (deleteRequested) {
                                if (mAutofillPaymentMethodsDelegate == null) {
                                    mAutofillPaymentMethodsDelegate =
                                            new AutofillPaymentMethodsDelegate(getProfile());
                                }
                                mAutofillPaymentMethodsDelegate.deleteSavedCvcs();
                            }
                        });
        dialog.show();
    }

    /**
     * Returns a pair of the number of types of financial accounts and the string to be displayed in
     * the settings page.
     */
    private Pair<Integer, String> getOtherFinancialAccountsTypes(
            PersonalDataManager personalDataManager) {
        return personalDataManager.getMaskedBankAccounts().length == 0
                ? new Pair<>(0, "")
                : new Pair<>(
                        1,
                        getResources()
                                .getString(R.string.settings_manage_other_financial_accounts_pix));
    }

    /** Show the page for managing other finiancial accounts. */
    private boolean showOtherFinancialAccountsFragment(Preference preference) {
        Bundle args = preference.getExtras();
        args.putString(
                FinancialAccountsManagementFragment.TITLE_KEY, preference.getTitle().toString());
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                getActivity(), FinancialAccountsManagementFragment.class, args);
        return true;
    }

    // TODO(b/369900711): Create a shared getManageServerPaymentMethodLink() for cards and IBANs.
    // Returns the URL for managing IBAN in GPay Web.
    private String getManageIbanLink(Iban iban) {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT)) {
            return AUTOFILL_MANAGE_PAYMENTS_SANDBOX_URL + "&id=" + iban.getInstrumentId();
        }
        return AUTOFILL_MANAGE_PAYMENTS_URL + "&id=" + iban.getInstrumentId();
    }

    public void setServerIbanManageLinkOpenerCallbackForTesting(Callback<String> callback) {
        mServerIbanManageLinkOpenerCallback = callback;
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManagerFactory.getForProfile(getProfile()).registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManagerFactory.getForProfile(getProfile()).unregisterDataObserver(this);
        super.onDestroyView();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mReauthenticatorBridge != null) {
            mReauthenticatorBridge.destroy();
            mReauthenticatorBridge = null;
        }
    }
}
