// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillFallbackSurfaceLauncher;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.SaveUpdateAddressProfilePromptMode;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorObserverForTest;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.plus_addresses.PlusAddressesUserActions;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

/** Autofill profiles fragment, which allows the user to edit autofill profiles. */
@NullMarked
public class AutofillProfilesFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver {
    private final Delegate mAddressEditorDelegate =
            new Delegate() {
                // User has either created a new address, or edited an existing address.
                // We should save changes in any case.
                @Override
                public void onDone(AutofillAddress address) {
                    PersonalDataManagerFactory.getForProfile(getProfile())
                            .setProfile(address.getProfile());
                    SettingsAutofillAndPaymentsObserver.getInstance()
                            .notifyOnAddressUpdated(address);
                    if (sObserverForTest != null) {
                        sObserverForTest.onEditorReadyToEdit();
                    }
                }

                // User canceled edited meaning that |autofillAddress| has stayed intact.
                @Override
                public void onCancel() {
                    if (sObserverForTest != null) {
                        sObserverForTest.onEditorReadyToEdit();
                    }
                }

                @Override
                public void onDelete(AutofillAddress address) {
                    String guid = address.getProfile().getGUID();
                    if (guid == null) {
                        return;
                    }
                    PersonalDataManagerFactory.getForProfile(getProfile()).deleteProfile(guid);
                    SettingsAutofillAndPaymentsObserver.getInstance().notifyOnAddressDeleted(guid);
                    if (sObserverForTest != null) {
                        sObserverForTest.onEditorReadyToEdit();
                    }
                }

                @Override
                public void onExternalEdit(AutofillProfile profile) {
                    switch (profile.getRecordType()) {
                        case RecordType.ACCOUNT_HOME:
                            CustomTabActivity.showInfoPage(
                                    getActivity(), GOOGLE_ACCOUNT_HOME_ADDRESS_EDIT_URL);
                            break;
                        case RecordType.ACCOUNT_WORK:
                            CustomTabActivity.showInfoPage(
                                    getActivity(), GOOGLE_ACCOUNT_WORK_ADDRESS_EDIT_URL);
                            break;
                        case RecordType.ACCOUNT_NAME_EMAIL:
                            CustomTabActivity.showInfoPage(
                                    getActivity(), GOOGLE_ACCOUNT_NAME_EMAIL_ADDRESS_EDIT_URL);
                            break;
                        case RecordType.ACCOUNT:
                        case RecordType.LOCAL_OR_SYNCABLE:
                            break;
                    }
                }
            };
    private static @Nullable EditorObserverForTest sObserverForTest;
    static final String PREF_NEW_PROFILE = "new_profile";
    static final String MANAGE_PLUS_ADDRESSES = "manage_plus_addresses";
    static final String SAVE_AND_FILL_ADDRESSES = "save_and_fill_addresses";
    static final String DISABLED_SETTINGS_INFO = "disabled_settings_info";

    public static final String GOOGLE_ACCOUNT_HOME_ADDRESS_EDIT_URL =
            "https://myaccount.google.com/address/home?utm_source=chrome&utm_campaign=manage_addresses";
    public static final String GOOGLE_ACCOUNT_WORK_ADDRESS_EDIT_URL =
            "https://myaccount.google.com/address/work?utm_source=chrome&utm_campaign=manage_addresses";
    public static final String GOOGLE_ACCOUNT_NAME_EMAIL_ADDRESS_EDIT_URL =
            "https://myaccount.google.com/personal-info?utm_source=chrome-settings&utm_medium=autofill";

    private @Nullable AddressEditorCoordinator mAddressEditor;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_addresses_settings_title));
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
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mAddressEditor != null) {
            mAddressEditor.onConfigurationChanged();
        }
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
    public void onStart() {
        super.onStart();
        // Always rebuild our list of profiles.  Although we could detect if profiles are added or
        // deleted (GUID list changes), the profile summary (name+addr) might be different.  To be
        // safe, we update all.
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    private void rebuildProfileList() {
        PreferenceScreen screen = getPreferenceScreen();
        screen.removeAll();
        screen.setOrderingAsAdded(true);
        if (disabledSettingsInThirdPartyMode()) {
            addDisabledSettingsInfoCard(screen);
        }
        addAutofillSwitch(screen);
        addProfilePreferences(screen);
        if (!disabledSettingsInThirdPartyMode()) {
            addAddAddressButton(screen);
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESSES_ENABLED)) {
            addPlusAddressesPreference(screen);
        }
    }

    /** Adds an information card if settings are disabled in third-party mode. */
    private void addDisabledSettingsInfoCard(PreferenceScreen screen) {
        CardWithButtonPreference disabledSettingsInfoPref =
                new CardWithButtonPreference(getStyledContext(), null);
        disabledSettingsInfoPref.setKey(DISABLED_SETTINGS_INFO);
        disabledSettingsInfoPref.setTitle(R.string.autofill_disable_settings_explanation_title);
        disabledSettingsInfoPref.setSummary(R.string.autofill_disable_settings_explanation);
        disabledSettingsInfoPref.setButtonText(
                getResources().getString(R.string.autofill_disable_settings_button_label));
        disabledSettingsInfoPref.setIconResource(R.drawable.ic_google_services_24dp);
        disabledSettingsInfoPref.setOnButtonClick(
                () -> {
                    SettingsNavigation settingsNavigation =
                            SettingsNavigationFactory.createSettingsNavigation();
                    settingsNavigation.startSettings(
                            getPreferenceManager().getContext(),
                            AutofillOptionsFragment.class,
                            AutofillOptionsFragment.createRequiredArgs(
                                    AutofillOptionsReferrer.AUTOFILL_PROFILES_FRAGMENT));
                });

        screen.addPreference(disabledSettingsInfoPref);
    }

    /** Adds the "Save and fill addresses" toggle. */
    private void addAutofillSwitch(PreferenceScreen screen) {
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        ChromeSwitchPreference autofillSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_profiles_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_profiles_toggle_sublabel);
        autofillSwitch.setChecked(personalDataManager.isAutofillProfileEnabled());
        autofillSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    personalDataManager.setAutofillProfileEnabled((boolean) newValue);
                    return true;
                });
        autofillSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return personalDataManager.isAutofillProfileManaged();
                    }

                    @Override
                    public boolean isPreferenceClickDisabled(Preference preference) {
                        return personalDataManager.isAutofillProfileManaged()
                                && !personalDataManager.isAutofillProfileEnabled();
                    }
                });
        // For testing.
        autofillSwitch.setKey(SAVE_AND_FILL_ADDRESSES);
        if (disabledSettingsInThirdPartyMode()) {
            autofillSwitch.setChecked(false);
            autofillSwitch.setEnabled(false);
        }

        screen.addPreference(autofillSwitch);
    }

    /** Adds a preference for each saved Autofill profile. */
    private void addProfilePreferences(PreferenceScreen screen) {
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        for (AutofillProfile profile : personalDataManager.getProfilesForSettings()) {
            // Add a preference for the profile.
            AutofillProfileEditorPreference pref =
                    new AutofillProfileEditorPreference(getStyledContext());
            pref.setTitle(profile.getInfo(FieldType.NAME_FULL));
            pref.setSummary(profile.getLabel());
            pref.setKey(String.valueOf(pref.getTitle())); // For testing.

            // Set the widget to display an icon indicating the profile's type: local, home or work.
            if (shouldShowLocalProfileIcon(profile)) {
                pref.setWidgetLayoutResource(R.layout.autofill_settings_local_profile_icon);
            } else if (profile.getRecordType() == RecordType.ACCOUNT_HOME) {
                pref.setWidgetLayoutResource(R.layout.autofill_settings_home_profile_icon);
            } else if (profile.getRecordType() == RecordType.ACCOUNT_WORK) {
                pref.setWidgetLayoutResource(R.layout.autofill_settings_work_profile_icon);
            }
            Bundle args = pref.getExtras();
            args.putString(AutofillEditorBase.AUTOFILL_GUID, profile.getGUID());

            screen.addPreference(pref);
        }
    }

    /**
     * Add 'Add address' button. Tapping on it brings up address editor which allows users to type
     * in new addresses.
     */
    private void addAddAddressButton(PreferenceScreen screen) {
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());

        if (personalDataManager.isAutofillProfileEnabled()) {
            AutofillProfileEditorPreference pref =
                    new AutofillProfileEditorPreference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(
                    SemanticColorUtils.getDefaultControlColorActive(getContext()),
                    PorterDuff.Mode.SRC_IN);
            pref.setIcon(plusIcon);
            pref.setTitle(R.string.autofill_create_profile);
            pref.setKey(PREF_NEW_PROFILE); // For testing.

            screen.addPreference(pref);
        }
    }

    /** Adds the "Manage plus addresses" link if the feature is enabled. */
    private void addPlusAddressesPreference(PreferenceScreen screen) {
        AutofillProfileEditorPreference pref =
                new AutofillProfileEditorPreference(getStyledContext());
        pref.setTitle(R.string.plus_address_settings_entry_title);
        pref.setSummary(R.string.plus_address_settings_entry_summary);
        pref.setKey(MANAGE_PLUS_ADDRESSES);

        screen.addPreference(pref);
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildProfileList();
        notifyPreferencesUpdated();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManagerFactory.getForProfile(getProfile()).registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManagerFactory.getForProfile(getProfile()).unregisterDataObserver(this);
        super.onDestroyView();
    }

    public static void setObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        EditorDialogView.setEditorObserverForTest(sObserverForTest);
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (!(preference instanceof AutofillProfileEditorPreference)) {
            super.onDisplayPreferenceDialog(preference);
            return;
        }

        AutofillProfileEditorPreference editorPreference =
                (AutofillProfileEditorPreference) preference;

        if (editorPreference.getKey().equals(MANAGE_PLUS_ADDRESSES)) {
            AutofillFallbackSurfaceLauncher.openManagePlusAddresses(getActivity(), getProfile());
            PlusAddressesUserActions.MANAGE_OPTION_ON_SETTINGS_SELECTED.log();
            return;
        }

        AutofillAddress autofillAddress = getAutofillAddress(editorPreference);
        if (autofillAddress == null) {
            mAddressEditor =
                    new AddressEditorCoordinator(
                            getActivity(),
                            mAddressEditorDelegate,
                            getProfile(),
                            /* saveToDisk= */ true);
            mAddressEditor.showEditorDialog();
        } else {
            mAddressEditor =
                    new AddressEditorCoordinator(
                            getActivity(),
                            mAddressEditorDelegate,
                            getProfile(),
                            autofillAddress,
                            SaveUpdateAddressProfilePromptMode.UPDATE_PROFILE,
                            /* saveToDisk= */ true);
            mAddressEditor.setAllowDelete(true);
            mAddressEditor.showEditorDialog();
        }
    }

    private @Nullable AutofillAddress getAutofillAddress(
            AutofillProfileEditorPreference preference) {
        String guid = preference.getGUID();
        if (guid == null) {
            return null;
        }
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        AutofillProfile profile = personalDataManager.getProfile(guid);
        if (profile == null) {
            return null;
        }
        return new AutofillAddress(getActivity(), profile, personalDataManager);
    }

    private boolean shouldShowLocalProfileIcon(AutofillProfile profile) {
        IdentityManager identityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(getProfile()));
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }
        if (profile.getRecordType() != RecordType.LOCAL_OR_SYNCABLE) {
            return false;
        }
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        return syncService == null
                || !syncService.getSelectedTypes().contains(UserSelectableType.AUTOFILL);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    EditorDialogView getEditorDialogForTest() {
        return assumeNonNull(mAddressEditor).getEditorDialogForTesting();
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "autofill_addresses";
    }

    private boolean disabledSettingsInThirdPartyMode() {
        return AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(
                                UserPrefs.get(getProfile()))
                        == AndroidAutofillAvailabilityStatus.AVAILABLE
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN);
    }
}
