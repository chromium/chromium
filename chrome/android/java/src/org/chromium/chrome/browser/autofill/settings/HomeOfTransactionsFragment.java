// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID;

import android.content.Context;
import android.os.Bundle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.password_manager.settings.PasswordsPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Home of Transactions fragment, the main entry point for all Autofill and Passwords settings. */
@NullMarked
public class HomeOfTransactionsFragment extends ChromeBaseSettingsFragment {

    public static final String PREF_PASSWORDS = "autofill_and_passwords_gpm";
    public static final String PREF_AUTOFILL_PAYMENTS = "autofill_and_passwords_payments";
    public static final String PREF_AUTOFILL_ADDRESSES = "autofill_and_passwords_addresses";
    public static final String PREF_AUTOFILL_IDENTITY_DOCS = "autofill_and_passwords_identity_docs";
    public static final String PREF_AUTOFILL_TRAVEL = "autofill_and_passwords_travel";
    public static final String PREF_AUTOFILL_SETTINGS = "autofill_and_passwords_settings";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_and_passwords_settings_title));

        requireActivity()
                .addMenuProvider(
                        new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);

        SettingsUtils.addPreferencesFromResource(this, R.xml.home_of_transactions_preferences);

        PasswordsPreference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setProfile(getProfile());
        passwordsPreference.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        passwordsPreference.setOnPreferenceClickListener(
                preference -> {
                    PasswordManagerLauncher.showPasswordSettings(
                            getContext(),
                            getProfile(),
                            ManagePasswordsReferrer.CHROME_SETTINGS_AUTOFILL_AND_PASSWORDS,
                            mModalDialogManagerSupplier.asNonNull().get(),
                            /* managePasskeys= */ false);
                    return true;
                });

        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsNavigationHelper.showAutofillCreditCardSettings(
                                        getActivity()));

        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsNavigationHelper.showAutofillProfileSettings(
                                        getActivity()));

        Preference identityDocsPref = findPreference(PREF_AUTOFILL_IDENTITY_DOCS);
        identityDocsPref.setVisible(shouldShowIdentityDocs());
        identityDocsPref.setOnPreferenceClickListener(
                preference ->
                        SettingsNavigationHelper.showAutofillIdentityDocsSettings(getActivity()));

        Preference travelPref = findPreference(PREF_AUTOFILL_TRAVEL);
        travelPref.setVisible(shouldShowTravel());
        travelPref.setOnPreferenceClickListener(
                preference -> SettingsNavigationHelper.showAutofillTravelSettings(getActivity()));

        findPreference(PREF_AUTOFILL_SETTINGS)
                .setOnPreferenceClickListener(
                        preference -> {
                            SettingsNavigationFactory.createSettingsNavigation()
                                    .startSettings(
                                            getContext(),
                                            AutofillOptionsFragment.class,
                                            AutofillOptionsFragment.createRequiredArgs(
                                                    AutofillOptionsReferrer
                                                            .AUTOFILL_AND_PASSWORDS_FRAGMENT));
                            return true;
                        });
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    private static boolean shouldShowIdentityDocs() {
        // TODO(crbug.com/482994257): Implement visibility logic for identity docs.
        return true;
    }

    private static boolean shouldShowTravel() {
        // TODO(crbug.com/482994258): Implement visibility logic for travel.
        return true;
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return UserPrefs.get(getProfile())
                            .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    // Password manager is always enabled, even if disabled by policy. In such case
                    // a disclaimer text is displayed.
                    return false;
                }
                return super.isPreferenceClickDisabled(preference);
            }
        };
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    HomeOfTransactionsFragment.class.getName(),
                    R.xml.home_of_transactions_preferences) {

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    boolean featureDisabled =
                            !ChromeFeatureList.isEnabled(YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID);
                    if (featureDisabled) {
                        indexData.removeEntry(getUniqueId(PREF_PASSWORDS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_PAYMENTS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_ADDRESSES));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SETTINGS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_IDENTITY_DOCS));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_TRAVEL));
                    } else {
                        if (!shouldShowIdentityDocs()) {
                            indexData.removeEntry(getUniqueId(PREF_AUTOFILL_IDENTITY_DOCS));
                        }
                        if (!shouldShowTravel()) {
                            indexData.removeEntry(getUniqueId(PREF_AUTOFILL_TRAVEL));
                        }
                    }
                }
            };

    @Initializer
    public void setModalDialogManagerSupplier(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }
}
