// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.sync;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.Passphrase;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption). Can be accessed from
 * {@link SyncAndServicesPreferences}.
 */
public class ManageSyncPreferences extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener {
    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";

    @VisibleForTesting
    public static final String PREF_SYNC_EVERYTHING = "sync_everything";
    @VisibleForTesting
    public static final String PREF_SYNC_AUTOFILL = "sync_autofill";
    @VisibleForTesting
    public static final String PREF_SYNC_BOOKMARKS = "sync_bookmarks";
    @VisibleForTesting
    public static final String PREF_SYNC_PAYMENTS_INTEGRATION = "sync_payments_integration";
    @VisibleForTesting
    public static final String PREF_SYNC_HISTORY = "sync_history";
    @VisibleForTesting
    public static final String PREF_SYNC_PASSWORDS = "sync_passwords";
    @VisibleForTesting
    public static final String PREF_SYNC_RECENT_TABS = "sync_recent_tabs";
    @VisibleForTesting
    public static final String PREF_SYNC_SETTINGS = "sync_settings";
    @VisibleForTesting
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";
    @VisibleForTesting
    public static final String PREF_ENCRYPTION = "encryption";
    @VisibleForTesting
    public static final String PREF_SYNC_MANAGE_DATA = "sync_manage_data";

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();

    private ChromeSwitchPreference mSyncEverything;
    private CheckBoxPreference mSyncAutofill;
    private CheckBoxPreference mSyncBookmarks;
    private CheckBoxPreference mSyncPaymentsIntegration;
    private CheckBoxPreference mSyncHistory;
    private CheckBoxPreference mSyncPasswords;
    private CheckBoxPreference mSyncRecentTabs;
    private CheckBoxPreference mSyncSettings;
    // Contains preferences for all sync data types.
    private CheckBoxPreference[] mSyncTypePreferences;

    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;

    private ProfileSyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.manage_sync_title);
        setHasOptionsMenu(true);

        PreferenceUtils.addPreferencesFromResource(this, R.xml.manage_sync_preferences);

        mSyncEverything = (ChromeSwitchPreference) findPreference(PREF_SYNC_EVERYTHING);
        mSyncEverything.setOnPreferenceChangeListener(this);

        mSyncAutofill = (CheckBoxPreference) findPreference(PREF_SYNC_AUTOFILL);
        mSyncBookmarks = (CheckBoxPreference) findPreference(PREF_SYNC_BOOKMARKS);
        mSyncPaymentsIntegration =
                (CheckBoxPreference) findPreference(PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncHistory = (CheckBoxPreference) findPreference(PREF_SYNC_HISTORY);
        mSyncPasswords = (CheckBoxPreference) findPreference(PREF_SYNC_PASSWORDS);
        mSyncRecentTabs = (CheckBoxPreference) findPreference(PREF_SYNC_RECENT_TABS);
        mSyncSettings = (CheckBoxPreference) findPreference(PREF_SYNC_SETTINGS);

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                SyncPreferenceUtils.toOnClickListener(this, this::onSyncEncryptionClicked));
        mManageSyncData = findPreference(PREF_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(SyncPreferenceUtils.toOnClickListener(
                this, () -> SyncPreferenceUtils.openSyncDashboard(getActivity())));

        mSyncTypePreferences =
                new CheckBoxPreference[] {mSyncAutofill, mSyncBookmarks, mSyncPaymentsIntegration,
                        mSyncHistory, mSyncPasswords, mSyncRecentTabs, mSyncSettings};
        for (CheckBoxPreference type : mSyncTypePreferences) {
            type.setOnPreferenceChangeListener(this);
        }

        if (Profile.getLastUsedProfile().isChild()) {
            mGoogleActivityControls.setSummary(
                    R.string.sign_in_google_activity_controls_summary_child_account);
        }

        // Prevent sync settings changes from taking effect until the user leaves this screen.
        mSyncSetupInProgressHandle = mProfileSyncService.getSetupInProgressHandle();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mSyncSetupInProgressHandle.close();
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
            HelpAndFeedback.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services),
                    Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void onStart() {
        super.onStart();
        mProfileSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public void onStop() {
        super.onStop();
        mProfileSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSyncPreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object o) {
        // A change to Preference state hasn't been applied yet. Defer
        // updateSyncStateFromSelectedModelTypes so it gets the updated state from isChecked().
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncStateFromSelectedModelTypes);
        return true;
    }

    /**
     * ProfileSyncService.SyncStateChangedListener implementation, listens to sync state changes.
     *
     * If the user has just turned on sync, this listener is needed in order to enable
     * the encryption settings once the engine has initialized.
     */
    @Override
    public void syncStateChanged() {
        // This is invoked synchronously from ProfileSyncService.setChosenDataTypes, postpone the
        // update to let updateSyncStateFromSelectedModelTypes finish saving the state.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncPreferences);
    }

    /**
     * Gets the current state of data types from {@link ProfileSyncService} and updates UI elements
     * from this state.
     */
    private void updateSyncPreferences() {
        String signedInAccountName = ChromeSigninController.get().getSignedInAccountName();
        if (signedInAccountName == null) {
            // May happen if account is removed from the device while this screen is shown.
            getActivity().finish();
            return;
        }

        mGoogleActivityControls.setOnPreferenceClickListener(SyncPreferenceUtils.toOnClickListener(
                this, () -> onGoogleActivityControlsClicked(signedInAccountName)));

        updateDataTypeState();
        updateEncryptionState();
    }

    /**
     * Gets the state from data type checkboxes and saves this state into {@link ProfileSyncService}
     * and {@link PersonalDataManager}.
     */
    private void updateSyncStateFromSelectedModelTypes() {
        mProfileSyncService.setChosenDataTypes(
                mSyncEverything.isChecked(), getSelectedModelTypes());
        // Note: mSyncPaymentsIntegration should be checked if mSyncEverything is checked, but if
        // mSyncEverything was just enabled, then that state may not have propagated to
        // mSyncPaymentsIntegration yet. See crbug.com/972863.
        PersonalDataManager.setPaymentsIntegrationEnabled(mSyncEverything.isChecked()
                || (mSyncPaymentsIntegration.isChecked() && mSyncAutofill.isChecked()));
        // Some calls to setChosenDataTypes don't trigger syncStateChanged, so schedule update here.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncPreferences);
    }

    /**
     * Update the encryption state.
     *
     * If sync's engine is initialized, the button is enabled and the dialog will present the
     * valid encryption options for the user. Otherwise, any encryption dialogs will be closed
     * and the button will be disabled because the engine is needed in order to know and
     * modify the encryption state.
     */
    private void updateEncryptionState() {
        boolean isEngineInitialized = mProfileSyncService.isEngineInitialized();
        mSyncEncryption.setEnabled(isEngineInitialized);
        mSyncEncryption.setSummary(null);
        if (!isEngineInitialized) {
            // If sync is not initialized, encryption state is unavailable and can't be changed.
            // Leave the button disabled and the summary empty. Additionally, close the dialogs in
            // case they were open when a stop and clear comes.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            return;
        }
        if (!mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes() && isAdded()) {
            mSyncEncryption.setSummary(
                    errorSummary(getString(R.string.sync_need_passphrase), getActivity()));
        }
    }

    /** Applies a span to the given string to give it an error color. */
    private static Spannable errorSummary(String string, Context context) {
        SpannableString summary = new SpannableString(string);
        summary.setSpan(new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.input_underline_error_color)),
                0, summary.length(), 0);
        return summary;
    }

    private Set<Integer> getSelectedModelTypes() {
        Set<Integer> types = new HashSet<>();
        if (mSyncAutofill.isChecked()) types.add(ModelType.AUTOFILL);
        if (mSyncBookmarks.isChecked()) types.add(ModelType.BOOKMARKS);
        if (mSyncHistory.isChecked()) types.add(ModelType.TYPED_URLS);
        if (mSyncPasswords.isChecked()) types.add(ModelType.PASSWORDS);
        if (mSyncRecentTabs.isChecked()) types.add(ModelType.PROXY_TABS);
        if (mSyncSettings.isChecked()) types.add(ModelType.PREFERENCES);
        return types;
    }

    private void displayPassphraseTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog =
                PassphraseTypeDialogFragment.create(mProfileSyncService.getPassphraseType(),
                        mProfileSyncService.getExplicitPassphraseTime(),
                        mProfileSyncService.isEncryptEverythingAllowed());
        dialog.show(ft, FRAGMENT_PASSPHRASE_TYPE);
        dialog.setTargetFragment(this, -1);
    }

    private void displayPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this).show(ft, FRAGMENT_ENTER_PASSPHRASE);
    }

    private void displayCustomPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseCreationDialogFragment dialog = new PassphraseCreationDialogFragment();
        dialog.setTargetFragment(this, -1);
        dialog.show(ft, FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void closeDialogIfOpen(String tag) {
        FragmentManager manager = getFragmentManager();
        if (manager == null) {
            // Do nothing if the manager doesn't exist yet; see http://crbug.com/480544.
            return;
        }
        DialogFragment df = (DialogFragment) manager.findFragmentByTag(tag);
        if (df != null) {
            df.dismiss();
        }
    }

    /** Returns whether the passphrase successfully decrypted the pending keys. */
    private boolean handleDecryption(String passphrase) {
        if (passphrase.isEmpty() || !mProfileSyncService.setDecryptionPassphrase(passphrase)) {
            return false;
        }
        // PassphraseDialogFragment doesn't handle closing itself, so do it here. This is not done
        // in updateSyncStateFromAndroidSyncSettings() because that happens onResume and possibly in
        // other cases where the dialog should stay open.
        closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        // Update our configuration UI.
        updateSyncPreferences();
        return true;
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!mProfileSyncService.isEngineInitialized()
                || !mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            // If the engine was shut down since the dialog was opened, or the passphrase isn't
            // required anymore, do nothing.
            return false;
        }
        return handleDecryption(passphrase);
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public void onPassphraseCanceled() {}

    /** Callback for PassphraseCreationDialogFragment.Listener */
    @Override
    public void onPassphraseCreated(String passphrase) {
        if (!mProfileSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }
        mProfileSyncService.enableEncryptEverything();
        mProfileSyncService.setEncryptionPassphrase(passphrase);
        // Save the current state of data types - this tells the sync engine to
        // apply our encryption configuration changes.
        updateSyncStateFromSelectedModelTypes();
    }

    /** Callback for PassphraseTypeDialogFragment.Listener */
    @Override
    public void onPassphraseTypeSelected(@Passphrase.Type int type) {
        if (!mProfileSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }

        boolean isAllDataEncrypted = mProfileSyncService.isEncryptEverythingEnabled();
        boolean isUsingSecondaryPassphrase = mProfileSyncService.isUsingSecondaryPassphrase();

        // The passphrase type should only ever be selected if the account doesn't have
        // full encryption enabled. Otherwise both options should be disabled.
        assert !isAllDataEncrypted;
        assert !isUsingSecondaryPassphrase;
        displayCustomPassphraseDialog();
    }

    private void onGoogleActivityControlsClicked(String signedInAccountName) {
        AppHooks.get().createGoogleActivityController().openWebAndAppActivitySettings(
                getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
    }

    private void onSyncEncryptionClicked() {
        if (!mProfileSyncService.isEngineInitialized()) return;

        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            displayPassphraseDialog();
        } else {
            displayPassphraseTypeDialog();
        }
    }

    /**
     * Gets the current state of data types from {@link ProfileSyncService} and updates the UI.
     */
    private void updateDataTypeState() {
        boolean syncEverything = mProfileSyncService.hasKeepEverythingSynced();
        mSyncEverything.setChecked(syncEverything);
        if (syncEverything) {
            for (CheckBoxPreference pref : mSyncTypePreferences) {
                pref.setChecked(true);
                pref.setEnabled(false);
            }
            return;
        }

        Set<Integer> syncTypes = mProfileSyncService.getChosenDataTypes();
        mSyncAutofill.setChecked(syncTypes.contains(ModelType.AUTOFILL));
        mSyncAutofill.setEnabled(true);
        mSyncBookmarks.setChecked(syncTypes.contains(ModelType.BOOKMARKS));
        mSyncBookmarks.setEnabled(true);
        mSyncHistory.setChecked(syncTypes.contains(ModelType.TYPED_URLS));
        mSyncHistory.setEnabled(true);
        mSyncPasswords.setChecked(syncTypes.contains(ModelType.PASSWORDS));
        mSyncPasswords.setEnabled(true);
        mSyncRecentTabs.setChecked(syncTypes.contains(ModelType.PROXY_TABS));
        mSyncRecentTabs.setEnabled(true);
        mSyncSettings.setChecked(syncTypes.contains(ModelType.PREFERENCES));
        mSyncSettings.setEnabled(true);

        // Payments integration requires AUTOFILL model type
        boolean syncAutofill = syncTypes.contains(ModelType.AUTOFILL);
        mSyncPaymentsIntegration.setChecked(
                syncAutofill && PersonalDataManager.isPaymentsIntegrationEnabled());
        mSyncPaymentsIntegration.setEnabled(syncAutofill);
    }
}
