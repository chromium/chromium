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

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SignOutCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
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
 * chrome://settings/syncSetup/advanced and parts of chrome://settings/syncSetup on desktop. This
 * fragment is accessible from the main settings view.
 */
public class ManageSyncSettings extends ChromeBaseSettingsFragment
        implements PassphraseDialogFragment.Delegate,
                PassphraseCreationDialogFragment.Listener,
                PassphraseTypeDialogFragment.Listener,
                Preference.OnPreferenceChangeListener,
                SyncService.SyncStateChangedListener,
                SyncErrorCardPreference.SyncErrorCardPreferenceListener,
                FragmentSettingsLauncher,
                IdentityErrorCardPreference.Listener {
    private static final String IS_FROM_SIGNIN_SCREEN = "ManageSyncSettings.isFromSigninScreen";

    @VisibleForTesting public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";

    @VisibleForTesting
    public static final String PREF_SYNC_ERROR_CARD_PREFERENCE = "sync_error_card";

    @VisibleForTesting
    public static final String PREF_IDENTITY_ERROR_CARD_PREFERENCE = "identity_error_card";

    @VisibleForTesting public static final String PREF_SYNCING_CATEGORY = "syncing_category";
    @VisibleForTesting public static final String PREF_SYNC_EVERYTHING = "sync_everything";
    @VisibleForTesting public static final String PREF_SYNC_AUTOFILL = "sync_autofill";
    @VisibleForTesting public static final String PREF_SYNC_BOOKMARKS = "sync_bookmarks";

    @VisibleForTesting
    public static final String PREF_SYNC_PAYMENTS_INTEGRATION = "sync_payments_integration";

    @VisibleForTesting public static final String PREF_SYNC_HISTORY = "sync_history";

    @VisibleForTesting
    public static final String PREF_SYNC_HISTORY_AND_TABS = "sync_history_and_tabs";

    @VisibleForTesting public static final String PREF_SYNC_PASSWORDS = "sync_passwords";
    @VisibleForTesting public static final String PREF_SYNC_READING_LIST = "sync_reading_list";
    @VisibleForTesting public static final String PREF_SYNC_RECENT_TABS = "sync_recent_tabs";
    @VisibleForTesting public static final String PREF_SYNC_SETTINGS = "sync_settings";
    @VisibleForTesting public static final String PREF_SYNC_APPS = "sync_apps";
    @VisibleForTesting public static final String PREF_TURN_OFF_SYNC = "turn_off_sync";
    private static final String PREF_ADVANCED_CATEGORY = "advanced_category";

    @VisibleForTesting
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";

    @VisibleForTesting public static final String PREF_ENCRYPTION = "encryption";
    @VisibleForTesting public static final String PREF_SYNC_REVIEW_DATA = "sync_review_data";

    @VisibleForTesting
    public static final String PREF_MANAGE_YOUR_GOOGLE_ACCOUNT = "manage_your_google_account";

    @VisibleForTesting
    public static final String PREF_MANAGE_ACCOUNTS_ON_THIS_DEVICE =
            "manage_accounts_on_this_device";

    @VisibleForTesting
    public static final String PREF_SEARCH_AND_BROWSE_CATEGORY = "search_and_browse_category";

    @VisibleForTesting
    public static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";

    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;
    private static final int REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 2;

    private SyncService mSyncService;
    private SettingsLauncher mSettingsLauncher;
    private SnackbarManager mSnackbarManager;

    private boolean mIsFromSigninScreen;

    private SyncErrorCardPreference mSyncErrorCardPreference;
    private PreferenceCategory mSyncingCategory;

    private ChromeSwitchPreference mSyncEverything;

    /**
     * Maps {@link UserSelectableType} to the corresponding preference.
     * `mSyncTypeCheckBoxPreferencesMap` is used for the sync settings, but
     * `mSyncTypeSwitchPreferencesMap` is used for the new account settings panel when
     * shouldReplaceSyncSettingsWithAccountSettings() returns true.
     */
    @Nullable private Map<Integer, ChromeBaseCheckBoxPreference> mSyncTypeCheckBoxPreferencesMap;

    @Nullable private Map<Integer, ChromeSwitchPreference> mSyncTypeSwitchPreferencesMap;

    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;

    private PreferenceCategory mSearchAndBrowseCategory;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;

    private SyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

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
        Profile profile = getProfile();
        mSyncService = SyncServiceFactory.getForProfile(profile);

        mIsFromSigninScreen =
                IntentUtils.safeGetBoolean(getArguments(), IS_FROM_SIGNIN_SCREEN, false);

        setHasOptionsMenu(true);

        if (shouldReplaceSyncSettingsWithAccountSettings()) {
            SettingsUtils.addPreferencesFromResource(
                    this, R.xml.unified_account_settings_preferences);

            IdentityErrorCardPreference identityErrorCardPreference =
                    (IdentityErrorCardPreference)
                            findPreference(PREF_IDENTITY_ERROR_CARD_PREFERENCE);
            identityErrorCardPreference.initialize(profile, this);

            mSyncTypeSwitchPreferencesMap = new HashMap<>();
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.AUTOFILL, findPreference(PREF_SYNC_AUTOFILL));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.BOOKMARKS, findPreference(PREF_SYNC_BOOKMARKS));
            // HISTORY and TABS are bundled in the same switch in the new settings panel.
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.HISTORY, findPreference(PREF_SYNC_HISTORY_AND_TABS));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.TABS, findPreference(PREF_SYNC_HISTORY_AND_TABS));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.PASSWORDS, findPreference(PREF_SYNC_PASSWORDS));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.PAYMENTS, findPreference(PREF_SYNC_PAYMENTS_INTEGRATION));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.PREFERENCES, findPreference(PREF_SYNC_SETTINGS));
            mSyncTypeSwitchPreferencesMap.put(
                    UserSelectableType.READING_LIST, findPreference(PREF_SYNC_READING_LIST));

            mSyncTypeSwitchPreferencesMap
                    .values()
                    .forEach(pref -> pref.setOnPreferenceChangeListener(this));

            Preference manageYourGoogleAccount = findPreference(PREF_MANAGE_YOUR_GOOGLE_ACCOUNT);
            manageYourGoogleAccount.setOnPreferenceClickListener(
                    SyncSettingsUtils.toOnClickListener(
                            this,
                            () -> {
                                SyncSettingsUtils.openGoogleMyAccount(getActivity());
                            }));

            Preference manageAccountsOnThisDevice =
                    findPreference(PREF_MANAGE_ACCOUNTS_ON_THIS_DEVICE);
            manageAccountsOnThisDevice.setOnPreferenceClickListener(
                    SyncSettingsUtils.toOnClickListener(
                            this, () -> SigninUtils.openSettingsForAllAccounts(getActivity())));
        } else {
            getActivity().setTitle(R.string.sync_category_title);

            SettingsUtils.addPreferencesFromResource(this, R.xml.manage_sync_preferences);

            mSyncErrorCardPreference =
                    (SyncErrorCardPreference) findPreference(PREF_SYNC_ERROR_CARD_PREFERENCE);
            mSyncErrorCardPreference.initialize(
                    ProfileDataCache.createWithDefaultImageSize(
                            getContext(), R.drawable.ic_sync_badge_error_20dp),
                    profile,
                    this);

            mSyncingCategory = (PreferenceCategory) findPreference(PREF_SYNCING_CATEGORY);

            mSyncEverything = (ChromeSwitchPreference) findPreference(PREF_SYNC_EVERYTHING);
            mSyncEverything.setOnPreferenceChangeListener(this);

            Preference turnOffSync = findPreference(PREF_TURN_OFF_SYNC);

            if (!mIsFromSigninScreen) {
                turnOffSync.setVisible(true);
                if (!profile.isChild()) {
                    // Non-child users have an option to sign out and turn off sync.  This is to
                    // ensure
                    // that revoking consents for sign in and sync does not require more steps than
                    // enabling them.
                    turnOffSync.setIcon(R.drawable.ic_signout_40dp);
                    turnOffSync.setTitle(R.string.sign_out_and_turn_off_sync);
                    turnOffSync.setOnPreferenceClickListener(
                            SyncSettingsUtils.toOnClickListener(
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

            mSyncTypeCheckBoxPreferencesMap = new HashMap<>();
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.AUTOFILL, findPreference(PREF_SYNC_AUTOFILL));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.BOOKMARKS, findPreference(PREF_SYNC_BOOKMARKS));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.HISTORY, findPreference(PREF_SYNC_HISTORY));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.PASSWORDS, findPreference(PREF_SYNC_PASSWORDS));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.PAYMENTS, findPreference(PREF_SYNC_PAYMENTS_INTEGRATION));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.PREFERENCES, findPreference(PREF_SYNC_SETTINGS));

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_APK_BACKUP_AND_RESTORE_BACKEND)) {
                mSyncTypeCheckBoxPreferencesMap.put(
                        UserSelectableType.APPS, findPreference(PREF_SYNC_APPS));
            } else {
                findPreference(PREF_SYNC_APPS).setVisible(false);
            }

            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.READING_LIST, findPreference(PREF_SYNC_READING_LIST));
            mSyncTypeCheckBoxPreferencesMap.put(
                    UserSelectableType.TABS, findPreference(PREF_SYNC_RECENT_TABS));

            mSyncTypeCheckBoxPreferencesMap
                    .values()
                    .forEach(pref -> pref.setOnPreferenceChangeListener(this));

            // Prevent sync settings changes from taking effect until the user leaves this screen.
            mSyncSetupInProgressHandle = mSyncService.getSetupInProgressHandle();

            mSearchAndBrowseCategory =
                    (PreferenceCategory) findPreference(PREF_SEARCH_AND_BROWSE_CATEGORY);

            mUrlKeyedAnonymizedData =
                    (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
            boolean urlKeyedAnonymizedDataShouldBeEnabled =
                    !UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(profile)
                            || UnifiedConsentServiceBridge
                                    .isUrlKeyedAnonymizedDataCollectionEnabled(profile);
            mUrlKeyedAnonymizedData.setChecked(urlKeyedAnonymizedDataShouldBeEnabled);
            mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(
                    new ChromeManagedPreferenceDelegate(profile) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return UnifiedConsentServiceBridge
                                    .isUrlKeyedAnonymizedDataCollectionManaged(profile);
                        }
                    });
        }

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.LINKED_SERVICES_SETTING)) {
            if (isEeaChoiceCountry()) {
                mGoogleActivityControls.setTitle(
                        R.string.sign_in_personalize_google_services_title_eea);
            } else {
                mGoogleActivityControls.setTitle(
                        R.string.sign_in_personalize_google_services_title);
            }
            mGoogleActivityControls.setSummary(
                    R.string.sign_in_personalize_google_services_summary);
        }

        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onSyncEncryptionClicked));

        Preference reviewSyncData = findPreference(PREF_SYNC_REVIEW_DATA);
        reviewSyncData.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this, () -> SyncSettingsUtils.openSyncDashboard(getActivity())));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (!shouldReplaceSyncSettingsWithAccountSettings()) {
            mSyncSetupInProgressHandle.close();
        }
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
            getHelpAndFeedbackLauncher()
                    .show(getActivity(), getString(R.string.help_context_sync_and_services), null);
            return true;
        }
        if (item.getItemId() == android.R.id.home) {
            return onBackToHome();
        }
        return false;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
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

    /** Handles when user clicks home button in menu to get back to home screen. */
    private boolean onBackToHome() {
        if (mIsFromSigninScreen) {
            RecordUserAction.record("Signin_Signin_BackOnAdvancedSyncSettings");
        }
        return false;
    }

    /**
     * Gets the current state of data types from {@link SyncService} and updates UI elements from
     * this state.
     */
    private void updateSyncPreferences() {
        String signedInAccountName =
                CoreAccountInfo.getEmailFrom(
                        IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(
                                        shouldReplaceSyncSettingsWithAccountSettings()
                                                ? ConsentLevel.SIGNIN
                                                : ConsentLevel.SYNC));
        // May happen if account is removed from the device while this screen is shown.
        if (signedInAccountName == null) {
            if (getActivity() != null) getActivity().finish();
            return;
        }

        mGoogleActivityControls.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this, () -> onGoogleActivityControlsClicked(signedInAccountName)));

        updateDataTypeState();
        updateEncryptionState();
    }

    /** Gets the state from data type checkboxes and saves this state into {@link SyncService}. */
    private void updateSyncStateFromSelectedTypes() {
        mSyncService.setSelectedTypes(
                shouldReplaceSyncSettingsWithAccountSettings()
                        ? false
                        : mSyncEverything.isChecked(),
                getUserSelectedTypes());

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
            setEncryptionErrorSummary(
                    mSyncService.isEncryptEverythingEnabled()
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
        return (shouldReplaceSyncSettingsWithAccountSettings()
                        ? mSyncTypeSwitchPreferencesMap
                        : mSyncTypeCheckBoxPreferencesMap)
                .entrySet().stream()
                        .filter(e -> e.getValue().isChecked())
                        .map(Map.Entry::getKey)
                        .collect(Collectors.toSet());
    }

    private void displayPassphraseTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog =
                PassphraseTypeDialogFragment.create(
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

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    private void onGoogleActivityControlsClicked(String signedInAccountName) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.LINKED_SERVICES_SETTING)
                && isEeaChoiceCountry()) {
            mSettingsLauncher.launchSettingsActivity(
                    getContext(), PersonalizeGoogleServicesSettings.class);
            RecordUserAction.record("Signin_AccountSettings_PersonalizeGoogleServicesClicked");
        } else {
            AppHooks.get()
                    .createGoogleActivityController()
                    .openWebAndAppActivitySettings(getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
        }
    }

    private void onSignOutAndTurnOffSyncClicked() {
        assert !getProfile().isChild();
        if (!IdentityServicesProvider.get()
                .getIdentityManager(getProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return;
        }
        SignOutCoordinator.startSignOutFlow(
                requireContext(),
                getProfile(),
                getChildFragmentManager(),
                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(),
                mSnackbarManager,
                SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                () -> {});
    }

    private void onTurnOffSyncClicked() {
        assert getProfile().isChild();
        if (!IdentityServicesProvider.get()
                .getIdentityManager(getProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return;
        }
        SignOutCoordinator.startSignOutFlow(
                requireContext(),
                getProfile(),
                getChildFragmentManager(),
                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(),
                mSnackbarManager,
                SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS,
                () -> {});
    }

    private void onSyncEncryptionClicked() {
        if (!mSyncService.isEngineInitialized()) return;

        if (mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            displayPassphraseDialog();
        } else if (mSyncService.isTrustedVaultKeyRequired()) {
            CoreAccountInfo primaryAccountInfo =
                    IdentityServicesProvider.get()
                            .getIdentityManager(getProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SYNC);
            if (primaryAccountInfo != null) {
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
            }
        } else {
            displayPassphraseTypeDialog();
        }
    }

    /** Gets the current state of data types from {@link SyncService} and updates the UI. */
    private void updateDataTypeState() {
        if (shouldReplaceSyncSettingsWithAccountSettings()) {
            Set<Integer> selectedSyncTypes = mSyncService.getSelectedTypes();

            for (Map.Entry<Integer, ChromeSwitchPreference> entry :
                    mSyncTypeSwitchPreferencesMap.entrySet()) {
                @UserSelectableType int type = entry.getKey();
                boolean enabled = !mSyncService.isTypeManagedByCustodian(type);
                boolean checked = selectedSyncTypes.contains(type);
                boolean managed = mSyncService.isTypeManagedByPolicy(type);

                if (type == UserSelectableType.TABS || type == UserSelectableType.HISTORY) {
                    // PREF_SYNC_HISTORY_AND_TABS toggle represents both History and Tabs in this
                    // case.
                    // History and Tabs should usually have the same value, but in some
                    // cases they may not, e.g. if one of them is disabled by policy. In that
                    // case, show the toggle as on if at least one of them is enabled. The
                    // toggle should reflect the value of the non-disabled type.
                    enabled =
                            !mSyncService.isTypeManagedByCustodian(UserSelectableType.TABS)
                                    || !mSyncService.isTypeManagedByCustodian(
                                            UserSelectableType.HISTORY);
                    checked =
                            selectedSyncTypes.contains(UserSelectableType.TABS)
                                    || selectedSyncTypes.contains(UserSelectableType.HISTORY);
                }

                ChromeSwitchPreference pref = entry.getValue();
                pref.setEnabled(enabled);
                pref.setChecked(checked);
                pref.setManagedPreferenceDelegate(
                        new ChromeManagedPreferenceDelegate(getProfile()) {
                            @Override
                            public boolean isPreferenceControlledByPolicy(Preference preference) {
                                return managed;
                            }
                        });
            }
            return;
        }
        boolean syncEverything = mSyncService.hasKeepEverythingSynced();
        mSyncEverything.setChecked(syncEverything);

        Set<Integer> selectedSyncTypes = mSyncService.getSelectedTypes();

        for (Map.Entry<Integer, ChromeBaseCheckBoxPreference> entry :
                mSyncTypeCheckBoxPreferencesMap.entrySet()) {
            @UserSelectableType int type = entry.getKey();
            ChromeBaseCheckBoxPreference pref = entry.getValue();
            boolean managed = mSyncService.isTypeManagedByPolicy(type);

            pref.setEnabled(!syncEverything && !mSyncService.isTypeManagedByCustodian(type));
            pref.setChecked(selectedSyncTypes.contains(type));

            pref.setManagedPreferenceDelegate(
                    new ChromeManagedPreferenceDelegate(getProfile()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return managed;
                        }
                    });
        }
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

    /**
     * Overrides FragmentSettingsLauncher.setSettingsLauncher to inject the App SettingsLauncher.
     *
     * @param settingsLauncher App SettingsLauncher instance.
     */
    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    // SyncErrorCardPreferenceListener implementation:
    @Override
    public boolean shouldSuppressSyncSetupIncomplete() {
        return mIsFromSigninScreen;
    }

    @Override
    public void onSyncErrorCardPrimaryButtonClicked() {
        assert !shouldReplaceSyncSettingsWithAccountSettings()
                : "Should not show on account settings page";
        assert mSyncService.hasSyncConsent();

        onErrorCardClicked(mSyncErrorCardPreference.getSyncError());
    }

    @Override
    public void onSyncErrorCardSecondaryButtonClicked() {
        assert !shouldReplaceSyncSettingsWithAccountSettings()
                : "Should not show on account settings page";
        assert mSyncService.hasSyncConsent();

        assert mSyncErrorCardPreference.getSyncError() == SyncError.SYNC_SETUP_INCOMPLETE;
        IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    @Override
    public void onIdentityErrorCardButtonClicked(@SyncError int error) {
        assert shouldReplaceSyncSettingsWithAccountSettings()
                : "Should not show on sync settings page";
        assert error != SyncError.SYNC_SETUP_INCOMPLETE : "Invalid error";
        assert error != SyncError.OTHER_ERRORS : "Not an identity error";
        onErrorCardClicked(error);
    }

    private void onErrorCardClicked(@SyncError int error) {
        Profile profile = getProfile();
        final CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(profile)
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        assert primaryAccountInfo != null;

        switch (error) {
            case SyncError.AUTH_ERROR:
                AccountManagerFacadeProvider.getInstance()
                        .updateCredentials(
                                CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo),
                                getActivity(),
                                null);
                return;
            case SyncError.CLIENT_OUT_OF_DATE:
                // Opens the client in play store for update.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(
                        Uri.parse(
                                "market://details?id="
                                        + ContextUtils.getApplicationContext().getPackageName()));
                startActivity(intent);
                return;
            case SyncError.OTHER_ERRORS:
                SignOutCoordinator.startSignOutFlow(
                        requireContext(),
                        profile,
                        getChildFragmentManager(),
                        ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(),
                        mSnackbarManager,
                        profile.isChild()
                                ? SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS
                                : SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                        () -> {});
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
                SyncSettingsUtils.openTrustedVaultRecoverabilityDegradedDialog(
                        this,
                        primaryAccountInfo,
                        REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED);
                return;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                mSyncService.setSyncRequested();
                mSyncService.setInitialSyncFeatureSetupComplete(
                        SyncFirstSetupCompleteSource.ADVANCED_FLOW_INTERRUPTED_TURN_SYNC_ON);
                return;
            case SyncError.UPM_BACKEND_OUTDATED:
                PasswordManagerHelper.launchGmsUpdate(getContext());
                return;
            case SyncError.NO_ERROR:
            default:
        }
    }

    private void confirmSettings() {
        RecordUserAction.record("Signin_Signin_ConfirmAdvancedSyncSettings");
        mSyncService.setInitialSyncFeatureSetupComplete(
                SyncFirstSetupCompleteSource.ADVANCED_FLOW_CONFIRM);

        Profile profile = getProfile();
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                profile, mUrlKeyedAnonymizedData.isChecked());
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram(profile);
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        Profile profile = getProfile();
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

    private boolean shouldReplaceSyncSettingsWithAccountSettings() {
        return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && !mSyncService.hasSyncConsent();
    }

    private boolean isEeaChoiceCountry() {
        TemplateUrlService templateUrlService =
                TemplateUrlServiceFactory.getForProfile(getProfile());
        return templateUrlService.isEeaChoiceCountry();
    }
}
