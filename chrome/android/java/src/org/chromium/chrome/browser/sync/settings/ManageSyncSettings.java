// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Dialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.ui.SignOutDialogFragment;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.ButtonCompat;

import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption). Corresponds to
 * chrome://settings/syncSetup/advanced and parts of chrome://settings/syncSetup on desktop.
 * With the MobileIdentityConsistency feature, this fragment is accessible from the main settings
 * view. If the feature is disabled, the entry point is in {@link SyncAndServicesSettings}.
 */
public class ManageSyncSettings extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener,
                   SettingsActivity.OnBackPressedListener,
                   SignOutDialogFragment.SignOutDialogListener,
                   SyncErrorCardPreference.SyncErrorCardPreferenceListener {
    private static final String IS_FROM_SIGNIN_SCREEN = "ManageSyncSettings.isFromSigninScreen";
    private static final String FRAGMENT_CANCEL_SYNC = "cancel_sync_dialog";
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";
    private static final String SIGN_OUT_DIALOG_TAG = "sign_out_dialog_tag";

    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";

    @VisibleForTesting
    public static final String PREF_SYNC_ERROR_CARD_PREFERENCE = "sync_error_card";
    @VisibleForTesting
    public static final String PREF_SYNCING_CATEGORY = "syncing_category";
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
    public static final String PREF_TURN_OFF_SYNC = "turn_off_sync";
    private static final String PREF_ADVANCED_CATEGORY = "advanced_category";
    @VisibleForTesting
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";
    @VisibleForTesting
    public static final String PREF_ENCRYPTION = "encryption";
    @VisibleForTesting
    public static final String PREF_SYNC_MANAGE_DATA = "sync_manage_data";
    @VisibleForTesting
    public static final String PREF_SEARCH_AND_BROWSE_CATEGORY = "search_and_browse_category";

    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";

    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();

    private boolean mIsFromSigninScreen;

    private SyncErrorCardPreference mSyncErrorCardPreference;
    private PreferenceCategory mSyncingCategory;

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

    private Preference mTurnOffSync;
    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;

    private PreferenceCategory mSearchAndBrowseCategory;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;

    private ProfileSyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    /**
     * Creates an argument bundle for this fragment.
     * @param isFromSigninScreen Whether the screen is started from the sign-in screen.
     */
    public static Bundle createArguments(boolean isFromSigninScreen) {
        Bundle result = new Bundle();
        result.putBoolean(IS_FROM_SIGNIN_SCREEN, isFromSigninScreen);
        return result;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        mIsFromSigninScreen =
                IntentUtils.safeGetBoolean(getArguments(), IS_FROM_SIGNIN_SCREEN, false);

        getActivity().setTitle(
                ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                        ? R.string.sync_category_title
                        : R.string.manage_sync_title);
        setHasOptionsMenu(true);
        if (mIsFromSigninScreen) {
            ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
            assert actionBar != null;
            actionBar.setHomeActionContentDescription(
                    R.string.prefs_manage_sync_settings_content_description);
            RecordUserAction.record("Signin_Signin_ShowAdvancedSyncSettings");
        }

        SettingsUtils.addPreferencesFromResource(this, R.xml.manage_sync_preferences);

        mSyncErrorCardPreference =
                (SyncErrorCardPreference) findPreference(PREF_SYNC_ERROR_CARD_PREFERENCE);
        mSyncErrorCardPreference.setSyncErrorCardPreferenceListener(this);

        mSyncingCategory = (PreferenceCategory) findPreference(PREF_SYNCING_CATEGORY);

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

        mTurnOffSync = findPreference(PREF_TURN_OFF_SYNC);
        mTurnOffSync.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onTurnOffSyncClicked));

        Profile profile = Profile.getLastUsedRegularProfile();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                && !mIsFromSigninScreen) {
            // Child profiles should not be able to sign out.
            mTurnOffSync.setVisible(!profile.isChild());
            findPreference(PREF_ADVANCED_CATEGORY).setVisible(true);

            /**
             * If MOBILE_IDENTITY_CONSISTENCY is disabled, sync data type states are retained even
             * if the user toggles 'Sync your Chrome data' off in {@link SyncAndServicesSettings}
             * page. This leads to an UI error that shows that all data types are enabled to sync
             * even though sync is shown as turned off in {@link ManageSyncSettings} page.
             * This state is impossible to reach if MOBILE_IDENTITY_CONSISTENCY is enabled.
             * TODO(https://crbug.com/1065029): This code will be removed after
             * MOBILE_IDENTITY_CONSISTENCY has been rolled out and existing users have been migrated
             */
            if (!ProfileSyncService.get().isSyncRequested()) {
                ProfileSyncService.get().setChosenDataTypes(false, new HashSet<>());
            }
        }

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onSyncEncryptionClicked));
        mManageSyncData = findPreference(PREF_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> SyncSettingsUtils.openSyncDashboard(getActivity())));

        mSyncTypePreferences =
                new CheckBoxPreference[] {mSyncAutofill, mSyncBookmarks, mSyncPaymentsIntegration,
                        mSyncHistory, mSyncPasswords, mSyncRecentTabs, mSyncSettings};
        for (CheckBoxPreference type : mSyncTypePreferences) {
            type.setOnPreferenceChangeListener(this);
        }

        if (profile.isChild()) {
            mGoogleActivityControls.setSummary(
                    R.string.sign_in_google_activity_controls_summary_child_account);
        }

        // Prevent sync settings changes from taking effect until the user leaves this screen.
        mSyncSetupInProgressHandle = mProfileSyncService.getSetupInProgressHandle();

        mSearchAndBrowseCategory =
                (PreferenceCategory) findPreference(PREF_SEARCH_AND_BROWSE_CATEGORY);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile));
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener((preference, newValue) -> {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    profile, (boolean) newValue);
            return true;
        });
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate((
                ChromeManagedPreferenceDelegate) (preference
                -> UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(profile)));
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                && item.getItemId() == android.R.id.home) {
            if (!mIsFromSigninScreen) return false; // Let Settings activity handle it.
            showCancelSyncDialog();
            return true;
        } else if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                || !mIsFromSigninScreen) {
            return super.onCreateView(inflater, container, savedInstanceState);
        }

        // Advanced sync consent flow - add a bottom bar and un-hide relevant preferences.
        ViewGroup result = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);
        inflater.inflate(R.layout.sync_and_services_bottom_bar, result, true);

        ButtonCompat cancelButton = result.findViewById(R.id.cancel_button);
        cancelButton.setOnClickListener(view -> cancelSync());
        ButtonCompat confirmButton = result.findViewById(R.id.confirm_button);
        confirmButton.setOnClickListener(view -> confirmSettings());

        mSearchAndBrowseCategory.setVisible(true);
        mSyncingCategory.setVisible(true);

        return result;
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
    public boolean onPreferenceChange(Preference preference, Object newValue) {
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
        String signedInAccountName = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC));
        if (signedInAccountName == null) {
            // May happen if account is removed from the device while this screen is shown.
            getActivity().finish();
            return;
        }

        mGoogleActivityControls.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> onGoogleActivityControlsClicked(signedInAccountName)));

        updateDataTypeState();
        updateEncryptionState();
    }

    /**
     * Gets the state from data type checkboxes and saves this state into {@link ProfileSyncService}
     * and {@link PersonalDataManager}.
     */
    private void updateSyncStateFromSelectedModelTypes() {
        Set<Integer> selectedModelTypes = getSelectedModelTypes();
        mProfileSyncService.setChosenDataTypes(mSyncEverything.isChecked(), selectedModelTypes);
        // Note: mSyncPaymentsIntegration should be checked if mSyncEverything is checked, but if
        // mSyncEverything was just enabled, then that state may not have propagated to
        // mSyncPaymentsIntegration yet. See crbug.com/972863.
        PersonalDataManager.setPaymentsIntegrationEnabled(mSyncEverything.isChecked()
                || (mSyncPaymentsIntegration.isChecked() && mSyncAutofill.isChecked()));

        // For child profiles sync should always be on.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                && !Profile.getLastUsedRegularProfile().isChild()) {
            boolean atLeastOneDataTypeEnabled =
                    mSyncEverything.isChecked() || selectedModelTypes.size() > 0;
            if (mProfileSyncService.isSyncRequested() && !atLeastOneDataTypeEnabled) {
                mProfileSyncService.setSyncRequested(false);
            } else if (!mProfileSyncService.isSyncRequested() && atLeastOneDataTypeEnabled) {
                mProfileSyncService.setSyncRequested(true);
            }
        }

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

        if (mProfileSyncService.isTrustedVaultKeyRequired()) {
            // The user cannot manually enter trusted vault keys, so it needs to gets treated as an
            // error.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            setEncryptionErrorSummary(mProfileSyncService.isEncryptEverythingEnabled()
                            ? R.string.sync_error_card_title
                            : R.string.password_sync_error_summary);
            return;
        }

        if (!mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes() && isAdded()) {
            setEncryptionErrorSummary(R.string.sync_need_passphrase);
        }
    }

    private void setEncryptionErrorSummary(@StringRes int stringId) {
        SpannableString summary = new SpannableString(getString(stringId));
        final int errorColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.input_underline_error_color);
        summary.setSpan(new ForegroundColorSpan(errorColor), 0, summary.length(), 0);
        mSyncEncryption.setSummary(summary);
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
        mProfileSyncService.setEncryptionPassphrase(passphrase);
        // Save the current state of data types - this tells the sync engine to
        // apply our encryption configuration changes.
        updateSyncStateFromSelectedModelTypes();
    }

    /** Callback for PassphraseTypeDialogFragment.Listener */
    @Override
    public void onPassphraseTypeSelected(@PassphraseType int type) {
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

    private void onTurnOffSyncClicked() {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount()) {
            return;
        }
        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                GAIAServiceType.GAIA_SERVICE_TYPE_NONE);

        SignOutDialogFragment signOutFragment =
                SignOutDialogFragment.create(GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        signOutFragment.setTargetFragment(this, 0);
        signOutFragment.show(getParentFragmentManager(), SIGN_OUT_DIALOG_TAG);
    }

    private void onSyncEncryptionClicked() {
        if (!mProfileSyncService.isEngineInitialized()) return;

        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            displayPassphraseDialog();
        } else if (mProfileSyncService.isTrustedVaultKeyRequired()) {
            CoreAccountInfo primaryAccountInfo =
                    IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SYNC);
            if (primaryAccountInfo != null) {
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
            }
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

    /**
     * Called upon completion of an activity started by a previous call to startActivityForResult()
     * via SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog().
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // Upon key retrieval completion, the keys in TrustedVaultClient could have changed. This is
        // done even if the user cancelled the flow (i.e. resultCode != RESULT_OK) because it's
        // harmless to issue a redundant notifyKeysChanged().
        if (requestCode == REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL) {
            TrustedVaultClient.get().notifyKeysChanged();
        }
    }

    @Override
    public boolean onBackPressed() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                || !mIsFromSigninScreen) {
            return false; // Let parent activity handle it.
        }
        showCancelSyncDialog();
        return true;
    }

    // SyncErrorCardPreferenceListener implementation:
    @Override
    public boolean shouldSuppressSyncSetupIncomplete() {
        return mIsFromSigninScreen;
    }

    @Override
    public void onSyncErrorCardPrimaryButtonClicked() {
        @SyncError
        int syncError = mSyncErrorCardPreference.getSyncError();
        Profile profile = Profile.getLastUsedRegularProfile();
        final CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get().getIdentityManager(profile).getPrimaryAccountInfo(
                        ConsentLevel.SYNC);
        assert primaryAccountInfo != null;

        switch (syncError) {
            case SyncError.ANDROID_SYNC_DISABLED:
                IntentUtils.safeStartActivity(
                        getActivity(), new Intent(Settings.ACTION_SYNC_SETTINGS));
                return;
            case SyncError.AUTH_ERROR:
                AccountManagerFacadeProvider.getInstance().updateCredentials(
                        CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo), getActivity(),
                        null);
                return;
            case SyncError.CLIENT_OUT_OF_DATE:
                // Opens the client in play store for update.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse("market://details?id="
                        + ContextUtils.getApplicationContext().getPackageName()));
                startActivity(intent);
                return;
            case SyncError.OTHER_ERRORS:
                SignOutDialogFragment signOutFragment =
                        SignOutDialogFragment.create(GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
                signOutFragment.setTargetFragment(this, 0);
                signOutFragment.show(getParentFragmentManager(), SIGN_OUT_DIALOG_TAG);
                return;
            case SyncError.PASSPHRASE_REQUIRED:
                displayPassphraseDialog();
                return;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
                return;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                mProfileSyncService.setSyncRequested(true);
                mProfileSyncService.setFirstSetupComplete(
                        SyncFirstSetupCompleteSource.ADVANCED_FLOW_INTERRUPTED_TURN_SYNC_ON);
                return;
            case SyncError.NO_ERROR:
            default:
                return;
        }
    }

    @Override
    public void onSyncErrorCardSecondaryButtonClicked() {
        assert mSyncErrorCardPreference.getSyncError() == SyncError.SYNC_SETUP_INCOMPLETE;
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    private void showCancelSyncDialog() {
        RecordUserAction.record("Signin_Signin_BackOnAdvancedSyncSettings");
        CancelSyncDialog dialog = new CancelSyncDialog();
        dialog.setTargetFragment(this, 0);
        dialog.show(getFragmentManager(), FRAGMENT_CANCEL_SYNC);
    }

    private void confirmSettings() {
        RecordUserAction.record("Signin_Signin_ConfirmAdvancedSyncSettings");
        ProfileSyncService.get().setFirstSetupComplete(
                SyncFirstSetupCompleteSource.ADVANCED_FLOW_CONFIRM);
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram(
                Profile.getLastUsedRegularProfile());
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .signOut(org.chromium.components.signin.metrics.SignoutReason
                                 .USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    /**
     * The dialog that offers the user to cancel sync. Only shown when
     * {@link ManageSyncSettings} is opened from the sign-in screen. Shown when the user
     * tries to close the settings page without confirming settings.
     */
    public static class CancelSyncDialog extends DialogFragment {
        public CancelSyncDialog() {
            // Fragment must have an empty public constructor
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                    .setTitle(R.string.cancel_sync_dialog_title)
                    .setMessage(R.string.cancel_sync_dialog_message)
                    .setNegativeButton(R.string.back, (dialog, which) -> onBackPressed())
                    .setPositiveButton(
                            R.string.cancel_sync_button, (dialog, which) -> onCancelSyncPressed())
                    .create();
        }

        private void onBackPressed() {
            RecordUserAction.record("Signin_Signin_CancelCancelAdvancedSyncSettings");
            dismiss();
        }

        public void onCancelSyncPressed() {
            RecordUserAction.record("Signin_Signin_ConfirmCancelAdvancedSyncSettings");
            ManageSyncSettings fragment = (ManageSyncSettings) getTargetFragment();
            fragment.cancelSync();
        }
    }

    // SignOutDialogListener implementation:
    @Override
    public void onSignOutClicked(boolean forceWipeUserData) {
        final Profile profile = Profile.getLastUsedRegularProfile();
        // In case sign-out happened while the dialog was displayed, we guard the sign out so
        // we do not hit a native crash.
        if (!IdentityServicesProvider.get().getIdentityManager(profile).hasPrimaryAccount()) return;

        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        IdentityServicesProvider.get().getSigninManager(profile).signOut(
                SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, new SigninManager.SignOutCallback() {
                    @Override
                    public void preWipeData() {
                        clearDataProgressDialog.show(
                                getChildFragmentManager(), CLEAR_DATA_PROGRESS_DIALOG_TAG);
                    }

                    @Override
                    public void signOutComplete() {
                        if (clearDataProgressDialog.isAdded()) {
                            clearDataProgressDialog.dismissAllowingStateLoss();
                        }
                    }
                }, forceWipeUserData);
    }
}
