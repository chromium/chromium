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
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.core.hardware.fingerprint.FingerprintManagerCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.autofill.MandatoryReauthAuthenticationFlowEvent;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.payments.AndroidPaymentAppFactory;

/**
 * Autofill credit cards fragment, which allows the user to edit credit cards and control
 * payment apps.
 */
public class AutofillPaymentMethodsFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver {
    // The Fido pref is used as a key on the settings toggle. This key helps in the retrieval of the
    // Fido toggle during tests.
    static final String PREF_FIDO = "fido";
    static final String PREF_DELETE_SAVED_CVCS = "delete_saved_cvcs";
    static final String PREF_MANDATORY_REAUTH = "mandatory_reauth";
    static final String PREF_SAVE_CVC = "save_cvc";
    private static final String PREF_PAYMENT_APPS = "payment_apps";

    static final String MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.AuthEvent.SettingsPage.EditCard";
    static final String MANDATORY_REAUTH_OPT_IN_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage.OptIn";
    static final String MANDATORY_REAUTH_OPT_OUT_HISTOGRAM =
            "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage.OptOut";

    @Nullable
    private ReauthenticatorBridge mReauthenticatorBridge;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_payment_methods);
        setHasOptionsMenu(true);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
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
            getHelpAndFeedbackLauncher().show(
                    getActivity(), getActivity().getString(R.string.help_context_autofill), null);
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

        ChromeSwitchPreference autofillSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_credit_cards_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_credit_cards_toggle_sublabel);
        autofillSwitch.setChecked(PersonalDataManager.isAutofillCreditCardEnabled());
        autofillSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PersonalDataManager.setAutofillCreditCardEnabled((boolean) newValue);
            return true;
        });
        autofillSwitch.setManagedPreferenceDelegate(new ChromeManagedPreferenceDelegate(
                getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged();
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged()
                        && !PersonalDataManager.isAutofillCreditCardEnabled();
            }
        });
        getPreferenceScreen().addPreference(autofillSwitch);

        if (isBiometricAvailable()
                && PersonalDataManager.getInstance().isFidoAuthenticationAvailable()
                && !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH)) {
            ChromeSwitchPreference fidoAuthSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            fidoAuthSwitch.setTitle(R.string.enable_credit_card_fido_auth_label);
            fidoAuthSwitch.setSummary(R.string.enable_credit_card_fido_auth_sublabel);
            fidoAuthSwitch.setKey(PREF_FIDO);
            fidoAuthSwitch.setChecked(PersonalDataManager.isAutofillCreditCardFidoAuthEnabled());
            fidoAuthSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
                PersonalDataManager.setAutofillCreditCardFidoAuthEnabled((boolean) newValue);
                return true;
            });
            getPreferenceScreen().addPreference(fidoAuthSwitch);
        }

        // TODO(crbug.com/1427216): Confirm with Product on the order of the toggles.
        // Don't show the toggle to enable mandatory reauth on automotive,
        // as the feature is always enabled for automotive builds.
        if (BuildInfo.getInstance().isAutomotive) {
            // The ReauthenticatorBridge is still needed for reauthentication to view/edit
            // payment methods.
            createReauthenticatorBridge();
        } else if (ChromeFeatureList.isEnabled(
                           ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH)) {
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
            saveCvcSwitch.setEnabled(PersonalDataManager.isAutofillCreditCardEnabled());
            saveCvcSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
                PersonalDataManager.setAutofillPaymentCvcStorage((boolean) newValue);
                return true;
            });
            getPreferenceScreen().addPreference(saveCvcSwitch);

            // When "Save And Fill Payments Methods" is disabled, we override this toggle's value to
            // off (but not change the underlying pref value). When "Save And Fill Payments Methods"
            // is ON, show the cvc storage pref value.
            saveCvcSwitch.setChecked(PersonalDataManager.isAutofillCreditCardEnabled()
                    && PersonalDataManager.isPaymentCvcStorageEnabled());

            // Add the deletion button for saved Cvc. Note that this button's presence doesn't
            // depend on "Save And Fill Payments Methods" value. Since we would like to allow user
            // to delete saved cvcs even when "Save And Fill Payments Methods" is disabled.
            // TODO(crbug.com/1474710): Conditionally show the deletion button based on whether
            // there is cvc stored.
            createDeleteSavedCvcs();
        }

        for (CreditCard card : PersonalDataManager.getInstance().getCreditCardsForSettings()) {
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
            card_pref.setIcon(getCardIcon(getStyledContext(), card.getCardArtUrl(),
                    card.getIssuerIconDrawableId(), AutofillUiUtils.CardIconSize.LARGE,
                    ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)));

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

        // Add 'Add credit card' button. Tap of it brings up card editor which allows users type in
        // new credit cards.
        if (PersonalDataManager.isAutofillCreditCardEnabled()) {
            Preference add_card_pref = new Preference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(SemanticColorUtils.getDefaultControlColorActive(getContext()),
                    PorterDuff.Mode.SRC_IN);
            add_card_pref.setIcon(plusIcon);
            add_card_pref.setTitle(R.string.autofill_create_credit_card);
            add_card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            getPreferenceScreen().addPreference(add_card_pref);
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
            mReauthenticatorBridge = ReauthenticatorBridge.create(DeviceAuthSource.AUTOFILL);
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
        // We always display the toggle, but the toggle is only enabled when Autofill credit
        // card is enabled AND the device supports biometric auth or screen lock. If either of
        // these is not met, we will grey out the toggle.
        boolean enableReauthSwitch = PersonalDataManager.isAutofillCreditCardEnabled()
                && mReauthenticatorBridge.canUseAuthenticationWithBiometricOrScreenLock();
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
                PersonalDataManager.isPaymentMethodsMandatoryReauthEnabled());
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
            return fingerprintManager != null && fingerprintManager.isHardwareDetected()
                    && fingerprintManager.hasEnrolledFingerprints();
        }
    }

    /** Handle preference changes from mandatory reauth toggle */
    private boolean onMandatoryReauthSwitchToggled(Preference preference, Object newValue) {
        assert preference.getKey().equals(PREF_MANDATORY_REAUTH);

        ChromeSwitchPreference mandatoryReauthSwitch = (ChromeSwitchPreference) preference;
        // If the user preference update is successful, toggle the switch to the success state.
        boolean userIntendedState = !mandatoryReauthSwitch.isChecked();
        String histogramName = userIntendedState ? MANDATORY_REAUTH_OPT_IN_HISTOGRAM
                                                 : MANDATORY_REAUTH_OPT_OUT_HISTOGRAM;
        RecordHistogram.recordEnumeratedHistogram(histogramName,
                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
        // We require user authentication every time user tries to change this
        // preference. Set useLastValidAuth=false to skip the grace period.
        mReauthenticatorBridge.reauthenticate(success -> {
            if (success) {
                // Only set the preference to new value when user passes the
                // authentication.
                PersonalDataManager.setAutofillPaymentMethodsMandatoryReauth((boolean) newValue);

                // When the preference is updated, the page is expected to refresh and show the
                // updated preference. Fallback if the page does not load.
                mandatoryReauthSwitch.setChecked(userIntendedState);
                RecordHistogram.recordEnumeratedHistogram(histogramName,
                        MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED,
                        MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
            } else {
                RecordHistogram.recordEnumeratedHistogram(histogramName,
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
     * @param preference The {@link Preference} for the local card.
     * @return true if the click was handled, false otherwise.
     */
    private boolean showLocalCardEditPageAfterAuthenticationIfRequired(Preference preference) {
        // If mandatory reauth is not enabled, just show the local card edit page. Note that
        // mandatory reauth is always enabled on automotive devices.
        boolean mandatoryReauthFeatureEnabled =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH)
                        || BuildInfo.getInstance().isAutomotive;

        if (!mandatoryReauthFeatureEnabled
                || !PersonalDataManager.isPaymentMethodsMandatoryReauthEnabled()) {
            showLocalCardEditPage(preference);
            return true;
        }

        // mReauthenticatorBridge should be initiated already when determining whether to show the
        // mandatory reauth toggle.
        assert mReauthenticatorBridge != null;
        RecordHistogram.recordEnumeratedHistogram(MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
        // When mandatory reauth is enabled, offer device authentication challenge.
        mReauthenticatorBridge.reauthenticate(success -> {
            // If authentication is successful, manually trigger the local card edit page. Else,
            // stay on this page.
            if (success) {
                RecordHistogram.recordEnumeratedHistogram(MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                        MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED,
                        MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
                showLocalCardEditPage(preference);
            } else {
                RecordHistogram.recordEnumeratedHistogram(MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                        MandatoryReauthAuthenticationFlowEvent.FLOW_FAILED,
                        MandatoryReauthAuthenticationFlowEvent.MAX_VALUE + 1);
            }
        });
        return true;
    }

    /**
     * Show the local card edit page for the given local card.
     * @param preference The {@link Preference} for the local card.
     */
    private void showLocalCardEditPage(Preference preference) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                getActivity(), AutofillLocalCardEditor.class, preference.getExtras());
    }

    /**
     * Create a clickable "Delete saved cvcs" button and add it to the preference screen.
     * No divider line above this preference.
     */
    private void createDeleteSavedCvcs() {
        ChromeBasePreference deleteSavedCvcs = new ChromeBasePreference(getStyledContext());
        deleteSavedCvcs.setKey(PREF_DELETE_SAVED_CVCS);
        SpannableString spannableString = new SpannableString(
                getResources().getString(R.string.autofill_settings_page_bulk_remove_cvc_label));
        spannableString.setSpan(new ForegroundColorSpan(getContext().getColor(
                                        R.color.default_text_color_link_baseline)),
                0, spannableString.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        deleteSavedCvcs.setSummary(spannableString);
        deleteSavedCvcs.setDividerAllowedAbove(false);
        // TODO(crbug.com/1474710): Add click listener.
        getPreferenceScreen().addPreference(deleteSavedCvcs);
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManager.getInstance().unregisterDataObserver(this);
        super.onDestroyView();
    }
}
