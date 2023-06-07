// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
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
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.Listener;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.widget.ButtonCompat;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Settings fragment to customize Sync options (data types, encryption). Corresponds to
 * chrome://settings/syncSetup/advanced and parts of chrome://settings/syncSetup on desktop.
 * This fragment is accessible from the main settings view.
 */
public class ManageSyncSettings extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   SyncService.SyncStateChangedListener, BackPressHelper.ObsoleteBackPressedHandler,
                   Listener, SyncErrorCardPreference.SyncErrorCardPreferenceListener,
                   FragmentHelpAndFeedbackLauncher {
    private static final String IS_FROM_SIGNIN_SCREEN = "ManageSyncSettings.isFromSigninScreen";
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

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
    public static final String PREF_SYNC_READING_LIST = "sync_reading_list";
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
    public static final String PREF_SYNC_REVIEW_DATA = "sync_review_data";
    @VisibleForTesting
    public static final String PREF_SEARCH_AND_BROWSE_CATEGORY = "search_and_browse_category";
    @VisibleForTesting
    public static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";

    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;
    private static final int REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 2;

    private SyncService mSyncService;

    private boolean mIsFromSigninScreen;

    private SyncErrorCardPreference mSyncErrorCardPreference;
    private PreferenceCategory mSyncingCategory;

    private ChromeSwitchPreference mSyncEverything;
    private ChromeBaseCheckBoxPreference mSyncPaymentsIntegration;
    // Maps {@link UserSelectableType} to the corresponding preference. There is no entry
    // for {@code mSyncPaymentsIntegration} because it does not correspond to a {@link
    // UserSelectableType}
    private Map<Integer, ChromeBaseCheckBoxPreference> mSyncTypePreferencesMap = new HashMap<>();

    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;

    private PreferenceCategory mSearchAndBrowseCategory;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;

    private SyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

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
        Profile profile = Profile.getLastUsedRegularProfile();
        mSyncService = SyncServiceFactory.getForProfile(profile);

        mIsFromSigninScreen =
                IntentUtils.safeGetBoolean(getArguments(), IS_FROM_SIGNIN_SCREEN, false);

        getActivity().setTitle(R.string.sync_category_title);
        setHasOptionsMenu(true);

        SettingsUtils.addPreferencesFromResource(this, R.xml.manage_sync_preferences);

        mSyncErrorCardPreference =
                (SyncErrorCardPreference) findPreference(PREF_SYNC_ERROR_CARD_PREFERENCE);
        mSyncErrorCardPreference.setSyncErrorCardPreferenceListener(this);

        mSyncingCategory = (PreferenceCategory) findPreference(PREF_SYNCING_CATEGORY);

        mSyncEverything = (ChromeSwitchPreference) findPreference(PREF_SYNC_EVERYTHING);
        mSyncEverything.setOnPreferenceChangeListener(this);

        Preference turnOffSync = findPreference(PREF_TURN_OFF_SYNC);

        if (!mIsFromSigninScreen) {
            turnOffSync.setVisible(true);
            if (!profile.isChild()) {
                // Non-child users have an option to sign out and turn off sync.  This is to ensure
                // that revoking consents for sign in and sync does not require more steps than
                // enabling them.
                turnOffSync.setIcon(R.drawable.ic_signout_40dp);
                turnOffSync.setTitle(R.string.sign_out_and_turn_off_sync);
                turnOffSync.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                        this, this::onSignOutAndTurnOffSyncClicked));
            } else {
                // Child users are force signed-in, so have an option which only turns off sync.
                turnOffSync.setIcon(R.drawable.ic_turn_off_sync_48dp);
                turnOffSync.setTitle(R.string.turn_off_sync);
                turnOffSync.setOnPreferenceClickListener(
                        SyncSettingsUtils.toOnClickListener(this, this::onTurnOffSyncClicked));
            }

            findPreference(PREF_ADVANCED_CATEGORY).setVisible(true);
        }

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onSyncEncryptionClicked));
        Preference reviewSyncData = findPreference(PREF_SYNC_REVIEW_DATA);
        reviewSyncData.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> SyncSettingsUtils.openSyncDashboard(getActivity())));

        mSyncTypePreferencesMap.put(
                UserSelectableType.AUTOFILL, findPreference(PREF_SYNC_AUTOFILL));
        mSyncTypePreferencesMap.put(
                UserSelectableType.BOOKMARKS, findPreference(PREF_SYNC_BOOKMARKS));
        mSyncTypePreferencesMap.put(UserSelectableType.HISTORY, findPreference(PREF_SYNC_HISTORY));
        mSyncTypePreferencesMap.put(
                UserSelectableType.PASSWORDS, findPreference(PREF_SYNC_PASSWORDS));
        mSyncTypePreferencesMap.put(
                UserSelectableType.READING_LIST, findPreference(PREF_SYNC_READING_LIST));
        mSyncTypePreferencesMap.put(UserSelectableType.TABS, findPreference(PREF_SYNC_RECENT_TABS));
        mSyncTypePreferencesMap.put(
                UserSelectableType.PREFERENCES, findPreference(PREF_SYNC_SETTINGS));

        mSyncTypePreferencesMap.values().forEach(pref -> pref.setOnPreferenceChangeListener(this));

        mSyncPaymentsIntegration =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncPaymentsIntegration.setOnPreferenceChangeListener(this);

        // Prevent sync settings changes from taking effect until the user leaves this screen.
        mSyncSetupInProgressHandle = mSyncService.getSetupInProgressHandle();

        mSearchAndBrowseCategory =
                (PreferenceCategory) findPreference(PREF_SEARCH_AND_BROWSE_CATEGORY);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        boolean urlKeyedAnonymizedDataShouldBeEnabled =
                !UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(profile)
                || UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
        mUrlKeyedAnonymizedData.setChecked(urlKeyedAnonymizedDataShouldBeEnabled);
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
        if (mIsFromSigninScreen) {
            ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
            assert actionBar != null;
            actionBar.setHomeActionContentDescription(
                    R.string.prefs_manage_sync_settings_content_description);
            RecordUserAction.record("Signin_Signin_ShowAdvancedSyncSettings");
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            mHelpAndFeedbackLauncher.show(
                    getActivity(), getString(R.string.help_context_sync_and_services), null);
            return true;
        }
        if (item.getItemId() == android.R.id.home) {
            return onBackPressed();
        }
        return false;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        if (!mIsFromSigninScreen) {
            return super.onCreateView(inflater, container, savedInstanceState);
        }

        // Advanced sync consent flow - add a bottom bar and un-hide relevant preferences.
        ViewGroup result = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);
        inflater.inflate(R.layout.manage_sync_settings_bottom_bar, result, true);

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
        mSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public void onStop() {
        super.onStop();
        mSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSyncPreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // A change to Preference state hasn't been applied yet. Defer
        // updateSyncStateFromSelectedTypes so it gets the updated state from
        // isChecked().
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::updateSyncStateFromSelectedTypes);
        return true;
    }

    /**
     * SyncService.SyncStateChangedListener implementation, listens to sync state changes.
     *
     * If the user has just turned on sync, this listener is needed in order to enable
     * the encryption settings once the engine has initialized.
     */
    @Override
    public void syncStateChanged() {
        // This is invoked synchronously from SyncService.setSelectedTypes, postpone the
        // update to let updateSyncStateFromSelectedTypes finish saving the state.
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::updateSyncPreferences);
    }

    /**
     * Gets the current state of data types from {@link SyncService} and updates UI elements
     * from this state.
     */
    private void updateSyncPreferences() {
        String signedInAccountName = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC));
        // May happen if account is removed from the device while this screen is shown.
        if (signedInAccountName == null) {
            if (getActivity() != null) getActivity().finish();
            return;
        }

        mGoogleActivityControls.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> onGoogleActivityControlsClicked(signedInAccountName)));

        updateDataTypeState();
        updateEncryptionState();
    }

    /**
     * Gets the state from data type checkboxes and saves this state into {@link SyncService}
     * and {@link PersonalDataManager}.
     */
    private void updateSyncStateFromSelectedTypes() {
        mSyncService.setSelectedTypes(mSyncEverything.isChecked(), getUserSelectedTypes());
        // Note: mSyncPaymentsIntegration should be checked if mSyncEverything is checked, but if
        // mSyncEverything was just enabled, then that state may not have propagated to
        // mSyncPaymentsIntegration yet. See crbug.com/972863.
        PersonalDataManager.setPaymentsIntegrationEnabled(mSyncEverything.isChecked()
                || (mSyncPaymentsIntegration.isChecked()
                        && mSyncTypePreferencesMap.get(UserSelectableType.AUTOFILL).isChecked()));

        // Some calls to setSelectedTypes don't trigger syncStateChanged, so schedule update here.
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::updateSyncPreferences);
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
        boolean isEngineInitialized = mSyncService.isEngineInitialized();
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

        if (mSyncService.isTrustedVaultKeyRequired()) {
            // The user cannot manually enter trusted vault keys, so it needs to gets treated as an
            // error.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            setEncryptionErrorSummary(mSyncService.isEncryptEverythingEnabled()
                            ? R.string.sync_error_card_title
                            : R.string.password_sync_error_summary);
            return;
        }

        if (!mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mSyncService.isPassphraseRequiredForPreferredDataTypes() && isAdded()) {
            setEncryptionErrorSummary(R.string.sync_need_passphrase);
        }
    }

    private void setEncryptionErrorSummary(@StringRes int stringId) {
        SpannableString summary = new SpannableString(getString(stringId));
        final int errorColor = getContext().getColor(R.color.input_underline_error_color);
        summary.setSpan(new ForegroundColorSpan(errorColor), 0, summary.length(), 0);
        mSyncEncryption.setSummary(summary);
    }

    private Set<Integer> getUserSelectedTypes() {
        return mSyncTypePreferencesMap.keySet()
                .stream()
                .filter(type -> mSyncTypePreferencesMap.get(type).isChecked())
                .collect(Collectors.toSet());
    }

    private void displayPassphraseTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog = PassphraseTypeDialogFragment.create(
                mSyncService.getPassphraseType(), mSyncService.isCustomPassphraseAllowed());
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
        if (passphrase.isEmpty() || !mSyncService.setDecryptionPassphrase(passphrase)) {
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
        if (!mSyncService.isEngineInitialized()
                || !mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
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
        if (!mSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }
        mSyncService.setEncryptionPassphrase(passphrase);
        // Save the current state of data types - this tells the sync engine to
        // apply our encryption configuration changes.
        updateSyncStateFromSelectedTypes();
    }

    /** Callback for PassphraseTypeDialogFragment.Listener */
    @Override
    public void onChooseCustomPassphraseRequested() {
        if (!mSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }

        // The passphrase type should only ever be selected if the account doesn't have
        // full encryption enabled. Otherwise both options should be disabled.
        assert !mSyncService.isEncryptEverythingEnabled();
        assert !mSyncService.isUsingExplicitPassphrase();
        displayCustomPassphraseDialog();
    }

    private void onGoogleActivityControlsClicked(String signedInAccountName) {
        AppHooks.get().createGoogleActivityController().openWebAndAppActivitySettings(
                getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
    }

    private void onSignOutAndTurnOffSyncClicked() {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return;
        }
        SignOutDialogCoordinator.show(requireContext(),
                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(), this,
                SignOutDialogCoordinator.ActionType.CLEAR_PRIMARY_ACCOUNT,
                GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void onTurnOffSyncClicked() {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return;
        }
        SignOutDialogCoordinator.show(requireContext(),
                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(), this,
                SignOutDialogCoordinator.ActionType.REVOKE_SYNC_CONSENT,
                GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void onSyncEncryptionClicked() {
        if (!mSyncService.isEngineInitialized()) return;

        if (mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            displayPassphraseDialog();
        } else if (mSyncService.isTrustedVaultKeyRequired()) {
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
     * Gets the current state of data types from {@link SyncService} and updates the UI.
     */
    private void updateDataTypeState() {
        boolean syncEverything = mSyncService.hasKeepEverythingSynced();
        mSyncEverything.setChecked(syncEverything);

        Set<Integer> selectedSyncTypes = mSyncService.getSelectedTypes();
        ChromeManagedPreferenceDelegate autofillChromeManagedPreferenceDelegate = null;

        for (Map.Entry<Integer, ChromeBaseCheckBoxPreference> entry :
                mSyncTypePreferencesMap.entrySet()) {
            @UserSelectableType
            int type = entry.getKey();
            ChromeBaseCheckBoxPreference pref = entry.getValue();
            boolean managed = mSyncService.isTypeManagedByPolicy(type);
            pref.setEnabled(!syncEverything);
            pref.setChecked(selectedSyncTypes.contains(type));

            ChromeManagedPreferenceDelegate managedDelegate =
                    new ChromeManagedPreferenceDelegate() {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return managed;
                        }
                    };
            pref.setManagedPreferenceDelegate(managedDelegate);
            if (type == UserSelectableType.AUTOFILL) {
                autofillChromeManagedPreferenceDelegate = managedDelegate;
            }
        }

        // Payments integration requires AUTOFILL user selectable type.
        mSyncPaymentsIntegration.setChecked(
                mSyncTypePreferencesMap.get(UserSelectableType.AUTOFILL).isChecked()
                && PersonalDataManager.isPaymentsIntegrationEnabled());
        mSyncPaymentsIntegration.setEnabled(!syncEverything
                && mSyncTypePreferencesMap.get(UserSelectableType.AUTOFILL).isChecked()
                && !Profile.getLastUsedRegularProfile().isChild());
        mSyncPaymentsIntegration.setManagedPreferenceDelegate(
                autofillChromeManagedPreferenceDelegate);
    }

    /**
     * Called upon completion of an activity started by a previous call to startActivityForResult()
     * via SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog() or
     * SyncSettingsUtils.openTrustedVaultRecoverabilityDegradedDialog().
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
        if (requestCode == REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED) {
            TrustedVaultClient.get().notifyRecoverabilityChanged();
        }
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug.com/1406012): Remove these metrics or introduce new metrics in other lifecycle
        //                          hooks because this method never consumes back event.
        if (mIsFromSigninScreen) {
            RecordUserAction.record("Signin_Signin_BackOnAdvancedSyncSettings");
        }
        return false;
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
                SignOutDialogCoordinator.show(requireContext(),
                        ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(), this,
                        profile.isChild()
                                ? SignOutDialogCoordinator.ActionType.REVOKE_SYNC_CONSENT
                                : SignOutDialogCoordinator.ActionType.CLEAR_PRIMARY_ACCOUNT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
                return;
            case SyncError.PASSPHRASE_REQUIRED:
                displayPassphraseDialog();
                return;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
                return;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                SyncSettingsUtils.openTrustedVaultRecoverabilityDegradedDialog(this,
                        primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED);
                return;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                mSyncService.setSyncRequested();
                mSyncService.setInitialSyncFeatureSetupComplete(
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

    private void confirmSettings() {
        RecordUserAction.record("Signin_Signin_ConfirmAdvancedSyncSettings");
        mSyncService.setInitialSyncFeatureSetupComplete(
                SyncFirstSetupCompleteSource.ADVANCED_FLOW_CONFIRM);

        Profile profile = Profile.getLastUsedRegularProfile();
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                profile, mUrlKeyedAnonymizedData.isChecked());
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram(profile);
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        Profile profile = Profile.getLastUsedRegularProfile();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        if (profile.isChild()) {
            // Child users cannot sign out, so we revoke the sync consent to return to the
            // previous state. This user won't have started syncing data yet, so there's need
            // need to wipe data before revoking consent.
            signinManager.revokeSyncConsent(
                    SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS, null, false);
        } else {
            signinManager.signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        }
        getActivity().finish();
    }

    // SignOutDialogListener implementation:
    @Override
    public void onSignOutClicked(boolean forceWipeUserData) {
        final Profile profile = Profile.getLastUsedRegularProfile();
        // In case sign-out happened while the dialog was displayed, we guard the sign out so
        // we do not hit a native crash.
        if (!IdentityServicesProvider.get().getIdentityManager(profile).hasPrimaryAccount(
                    ConsentLevel.SYNC)) {
            return;
        }

        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        SigninManager.SignOutCallback dataWipeCallback = new SigninManager.SignOutCallback() {
            @Override
            public void preWipeData() {
                clearDataProgressDialog.show(
                        getChildFragmentManager(), CLEAR_DATA_PROGRESS_DIALOG_TAG);
            }

            @Override
            public void signOutComplete() {
                // TODO(crbug.com/1313527): deal with both the following edge cases (currently
                // this code only deals with 1):
                //
                // 1) The parent activity showing the dialog is dismissed before signout completes.
                // 2) The signout completes before the dialog is added.
                if (clearDataProgressDialog.isAdded()) {
                    clearDataProgressDialog.dismissAllowingStateLoss();
                }
            }
        };

        if (profile.isChild()) {
            // Call through to PrimaryAccountMutatorImpl::RevokeSyncConsent().
            IdentityServicesProvider.get().getSigninManager(profile).revokeSyncConsent(
                    SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS, dataWipeCallback,
                    forceWipeUserData);
        } else {
            IdentityServicesProvider.get().getSigninManager(profile).signOut(
                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, dataWipeCallback,
                    forceWipeUserData);
        }
    }

    @Override
    public void setHelpAndFeedbackLauncher(HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
    }
}
