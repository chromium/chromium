// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.payments.BnplIssuerForSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/** Preferences fragment to allow users to manage Buy Now Pay Later application settings. */
@NullMarked
public class AutofillBuyNowPayLaterFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver,
                Preference.OnPreferenceClickListener,
                Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    static final String PREF_KEY_ENABLE_BUY_NOW_PAY_LATER = "enable_buy_now_pay_later";

    @VisibleForTesting
    static final String BNPL_ISSUER_TERMS_CLICKED_USER_ACTION = "Bnpl_IssuerTermsClicked";

    @VisibleForTesting static final String BNPL_TOGGLED_OFF_USER_ACTION = "Bnpl_ToggledOff";
    @VisibleForTesting static final String BNPL_TOGGLED_ON_USER_ACTION = "Bnpl_ToggledOn";

    @VisibleForTesting static final String PREF_KEY_BNPL_ISSUER_TERM = "bnpl_issuers_term_key";
    @VisibleForTesting static final String PREF_LIST_TERMS_URL = "bnpl_issuers_terms_url";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private PersonalDataManager mPersonalDataManager;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_bnpl_settings_label));
        setHasOptionsMenu(true);
        // Create blank preference screen.
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
            getHelpAndFeedbackLauncher()
                    .show(
                            getActivity(),
                            getActivity().getString(R.string.help_context_autofill),
                            /* url= */ null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Rebuild the preference list in case any of the underlying data has been updated.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);
        createBuyNowPayLaterSwitch();
        if (mPersonalDataManager.isBuyNowPayLaterEnabled()) {
            createPreferencesForBnplTerms();
        }
    }

    private void createBuyNowPayLaterSwitch() {
        ChromeSwitchPreference buyNowPayLaterSwitch =
                new ChromeSwitchPreference(getStyledContext());
        buyNowPayLaterSwitch.setTitle(R.string.autofill_bnpl_settings_label);
        buyNowPayLaterSwitch.setSummary(R.string.autofill_bnpl_settings_toggle_sublabel);
        buyNowPayLaterSwitch.setKey(PREF_KEY_ENABLE_BUY_NOW_PAY_LATER);
        buyNowPayLaterSwitch.setChecked(mPersonalDataManager.isBuyNowPayLaterEnabled());
        buyNowPayLaterSwitch.setOnPreferenceChangeListener(this);
        getPreferenceScreen().addPreference(buyNowPayLaterSwitch);
    }

    private void createPreferencesForBnplTerms() {
        for (BnplIssuerForSettings issuer : mPersonalDataManager.getBnplIssuersForSettings()) {
            // Add a preference for the BNPL issuer.
            ChromeBasePreference issuerPref = new ChromeBasePreference(getStyledContext());
            issuerPref.setDividerAllowedAbove(true);
            issuerPref.setDividerAllowedBelow(true);
            issuerPref.setTitle(issuer.getDisplayName());
            issuerPref.setKey(PREF_KEY_BNPL_ISSUER_TERM);

            // Add BNPL issuer site redirect.
            Bundle args = issuerPref.getExtras();
            args.putString(
                    PREF_LIST_TERMS_URL,
                    AutofillUiUtils.getManagePaymentMethodUrlForInstrumentId(
                            issuer.getInstrumentId()));
            issuerPref.setOnPreferenceClickListener(this);

            // Set BNPL issuer icon.
            issuerPref.setIcon(
                    AppCompatResources.getDrawable(getStyledContext(), issuer.getIconId()));

            // Add GPay icon.
            issuerPref.setWidgetLayoutResource(R.layout.autofill_server_data_label);

            getPreferenceScreen().addPreference(issuerPref);
        }
    }

    private void openUrlInCct(String url) {
        new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(getContext(), Uri.parse(url));
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        openUrlInCct(assumeNonNull(preference.getExtras().getString(PREF_LIST_TERMS_URL)));
        RecordUserAction.record(BNPL_ISSUER_TERMS_CLICKED_USER_ACTION);
        return true;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        boolean prefEnabled = (boolean) newValue;
        mPersonalDataManager.setBuyNowPayLater(prefEnabled);
        RecordUserAction.record(
                prefEnabled ? BNPL_TOGGLED_ON_USER_ACTION : BNPL_TOGGLED_OFF_USER_ACTION);
        return true;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
