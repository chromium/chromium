// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.PlusAddressesHelper;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorObserverForTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

/** Autofill profiles fragment, which allows the user to edit autofill profiles. */
public class AutofillProfilesFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver {
    private Delegate mAddressEditorDelegate =
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
            };
    private static EditorObserverForTest sObserverForTest;
    static final String PREF_NEW_PROFILE = "new_profile";
    static final String MANAGE_PLUS_ADDRESSES = "manage_plus_addresses";
    private @Nullable AddressEditorCoordinator mAddressEditor;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
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
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
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
    public void onResume() {
        super.onResume();
        // Always rebuild our list of profiles.  Although we could detect if profiles are added or
        // deleted (GUID list changes), the profile summary (name+addr) might be different.  To be
        // safe, we update all.
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    private void rebuildProfileList() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

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
        getPreferenceScreen().addPreference(autofillSwitch);

        for (AutofillProfile profile : personalDataManager.getProfilesForSettings()) {
            // Add a preference for the profile.
            Preference pref = new AutofillProfileEditorPreference(getStyledContext());
            pref.setTitle(profile.getFullName());
            pref.setSummary(profile.getLabel());
            pref.setKey(pref.getTitle().toString()); // For testing.
            if (shouldShowLocalProfileIcon(profile)) {
                // Conditionally set local profile icon for address profiles that are neither
                // synced, nor saved in the account.
                pref.setWidgetLayoutResource(R.layout.autofill_local_profile_icon);
            }
            Bundle args = pref.getExtras();
            args.putString(AutofillEditorBase.AUTOFILL_GUID, profile.getGUID());
            getPreferenceScreen().addPreference(pref);
        }

        // Add 'Add address' button. Tap of it brings up address editor which allows users type in
        // new addresses.
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

            getPreferenceScreen().addPreference(pref);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESS_ANDROID_SETTINGS_ENTRY)) {
            AutofillProfileEditorPreference pref =
                    new AutofillProfileEditorPreference(getStyledContext());
            pref.setTitle(R.string.plus_address_settings_entry_title);
            pref.setSummary(R.string.plus_address_settings_entry_summary);
            pref.setKey(MANAGE_PLUS_ADDRESSES);

            getPreferenceScreen().addPreference(pref);
        }
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
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

    public static void setObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        EditorDialogView.setEditorObserverForTest(sObserverForTest);
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (!(preference instanceof AutofillProfileEditorPreference)) {
            super.onDisplayPreferenceDialog(preference);
            return;
        }

        if (preference.getKey().equals(MANAGE_PLUS_ADDRESSES)) {
            PlusAddressesHelper.openManagePlusAddresses(
                    getActivity(), PlusAddressesHelper.getPlusAddressManagementUrl());
            return;
        }

        AutofillAddress autofillAddress =
                getAutofillAddress((AutofillProfileEditorPreference) preference);
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
                            UPDATE_EXISTING_ADDRESS_PROFILE,
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
        if (!IdentityServicesProvider.get()
                .getIdentityManager(getProfile())
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }
        if (profile.getRecordType() == RecordType.ACCOUNT) {
            return false;
        }
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE)) {
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
        return mAddressEditor.getEditorDialogForTesting();
    }
}
