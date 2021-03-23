// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Dialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ButtonCompat;

/**
 * WARNING: This class will be REMOVED after MobileIdentityConsistency launches.
 * SyncAndServicesAndSettings is a view in settings containing three sections: "User" (information
 * about the signed-in account), "Sync" (entry point to ManageSyncSettings and toggle to
 * enable/disable sync) and "Other Google services" (toggles controlling a variety of features
 * employing Google services, such as search autocomplete). With the MobileIdentityConsistency
 * feature, this view disappears and the "Sync" and "Google services" sections are split into
 * separate views, accessible directly from top-level settings. The "User" section disappears.
 */
public class SyncAndServicesSettings extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener,
                   SettingsActivity.OnBackPressedListener {
    private static final String IS_FROM_SIGNIN_SCREEN =
            "SyncAndServicesPreferences.isFromSigninScreen";

    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    private static final String FRAGMENT_CANCEL_SYNC = "cancel_sync_dialog";

    private static final String PREF_USER_CATEGORY = "user_category";
    private static final String PREF_SIGNIN = "sign_in";
    private static final String PREF_MANAGE_YOUR_GOOGLE_ACCOUNT = "manage_your_google_account";

    @VisibleForTesting
    public static final String PREF_SYNC_CATEGORY = "sync_category";
    @VisibleForTesting
    public static final String PREF_SYNC_ERROR_CARD = "sync_error_card";
    private static final String PREF_SYNC_DISABLED_BY_ADMINISTRATOR =
            "sync_disabled_by_administrator";
    @VisibleForTesting
    public static final String PREF_SYNC_REQUESTED = "sync_requested";
    private static final String PREF_MANAGE_SYNC = "manage_sync";

    private static final String PREF_SERVICES_CATEGORY = "services_category";
    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_NAVIGATION_ERROR = "navigation_error";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";
    @VisibleForTesting
    public static final String PREF_AUTOFILL_ASSISTANT = "autofill_assistant";
    @VisibleForTesting
    public static final String PREF_AUTOFILL_ASSISTANT_SUBSECTION = "autofill_assistant_subsection";
    @VisibleForTesting
    public static final String PREF_METRICS_SETTINGS = "metrics_settings";

    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();
    private final PrefService mPrefService = UserPrefs.get(getProfile());
    private final PrivacyPreferencesManagerImpl mPrivacyPrefManager =
            PrivacyPreferencesManagerImpl.getInstance();
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    private boolean mIsFromSigninScreen;

    private SignInPreference mSigninPreference;
    private Preference mManageYourGoogleAccount;

    private PreferenceCategory mSyncCategory;
    private Preference mSyncErrorCard;
    private Preference mSyncDisabledByAdministrator;
    private ChromeBasePreference mManageSync;
    private ChromeSwitchPreference mSyncRequested;

    private ChromeSwitchPreference mSearchSuggestions;
    private ChromeSwitchPreference mNavigationError;
    private ChromeSwitchPreference mUsageAndCrashReporting;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;
    private @Nullable ChromeSwitchPreference mAutofillAssistant;
    private @Nullable Preference mContextualSearch;

    private ProfileSyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    private @SyncError int mCurrentSyncError = SyncError.NO_ERROR;

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

        getActivity().setTitle(R.string.prefs_sync_and_services);
        setHasOptionsMenu(true);
        if (mIsFromSigninScreen) {
            ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
            assert actionBar != null;
            actionBar.setHomeActionContentDescription(
                    R.string.prefs_sync_and_services_content_description);
            RecordUserAction.record("Signin_Signin_ShowAdvancedSyncSettings");
        }

        SettingsUtils.addPreferencesFromResource(this, R.xml.sync_and_services_preferences);

        mSigninPreference = (SignInPreference) findPreference(PREF_SIGNIN);
        mManageYourGoogleAccount = findPreference(PREF_MANAGE_YOUR_GOOGLE_ACCOUNT);
        mManageYourGoogleAccount.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> SyncSettingsUtils.openGoogleMyAccount(getActivity())));

        mSyncCategory = (PreferenceCategory) findPreference(PREF_SYNC_CATEGORY);
        mSyncErrorCard = findPreference(PREF_SYNC_ERROR_CARD);
        mSyncErrorCard.setIcon(UiUtils.getTintedDrawable(
                getActivity(), R.drawable.ic_sync_error_legacy_40dp, R.color.default_red));
        mSyncErrorCard.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onSyncErrorCardClicked));
        mSyncDisabledByAdministrator = findPreference(PREF_SYNC_DISABLED_BY_ADMINISTRATOR);
        mSyncDisabledByAdministrator.setIcon(
                ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        mSyncRequested = (ChromeSwitchPreference) findPreference(PREF_SYNC_REQUESTED);
        mSyncRequested.setOnPreferenceChangeListener(this);
        mManageSync = (ChromeBasePreference) findPreference(PREF_MANAGE_SYNC);

        mSearchSuggestions = (ChromeSwitchPreference) findPreference(PREF_SEARCH_SUGGESTIONS);
        mSearchSuggestions.setOnPreferenceChangeListener(this);
        mSearchSuggestions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mNavigationError = (ChromeSwitchPreference) findPreference(PREF_NAVIGATION_ERROR);
        mNavigationError.setOnPreferenceChangeListener(this);
        mNavigationError.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        PreferenceCategory servicesCategory =
                (PreferenceCategory) findPreference(PREF_SERVICES_CATEGORY);

        // If the metrics-settings-android flag is not enabled, remove the corresponding element.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.METRICS_SETTINGS_ANDROID)) {
            removePreference(servicesCategory, findPreference(PREF_METRICS_SETTINGS));
        }

        mUsageAndCrashReporting =
                (ChromeSwitchPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        mUsageAndCrashReporting.setOnPreferenceChangeListener(this);
        mUsageAndCrashReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener(this);
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mAutofillAssistant = (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_ASSISTANT);
        Preference autofillAssistantSubsection = findPreference(PREF_AUTOFILL_ASSISTANT_SUBSECTION);
        // Assistant autofill/voicesearch both live in the sub-section. If either one of them is
        // enabled, then the subsection should show.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)) {
            removePreference(servicesCategory, mAutofillAssistant);
            mAutofillAssistant = null;
            autofillAssistantSubsection.setVisible(true);
        } else if (shouldShowAutofillAssistantPreference()) {
            mAutofillAssistant.setOnPreferenceChangeListener(this);
            mAutofillAssistant.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        } else {
            removePreference(servicesCategory, mAutofillAssistant);
            mAutofillAssistant = null;
        }

        mContextualSearch = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (!ContextualSearchFieldTrial.isEnabled()) {
            removePreference(servicesCategory, mContextualSearch);
            mContextualSearch = null;
        }

        // Prevent sync settings changes from taking effect until the user leaves this screen.
        mSyncSetupInProgressHandle = mProfileSyncService.getSetupInProgressHandle();

        updatePreferences();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // The user might also sign out from this page. Check to see if the user is signed in
        // before going through this flow.
        if (wasSigninFlowInterrupted()
                && mSigninPreference.getState() == SignInPreference.State.SIGNED_IN) {
            // If the setup flow was previously interrupted, and now the user dismissed the page
            // without turning sync on, then mark first setup as complete (so that we won't show the
            // error again), but turn sync off.
            assert !mSyncRequested.isChecked();
            SyncSettingsUtils.enableSync(false);
            mProfileSyncService.setFirstSetupComplete(
                    SyncFirstSetupCompleteSource.ADVANCED_FLOW_INTERRUPTED_LEAVE_SYNC_OFF);
        }
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
        if (item.getItemId() == android.R.id.home) {
            if (!mIsFromSigninScreen) return false; // Let Settings activity handle it.
            showCancelSyncDialog();
            return true;
        } else if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services), getProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        ViewGroup result = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);
        if (mIsFromSigninScreen) {
            inflater.inflate(R.layout.sync_and_services_bottom_bar, result, true);
            ButtonCompat cancelButton = result.findViewById(R.id.cancel_button);
            cancelButton.setOnClickListener(view -> cancelSync());

            ButtonCompat confirmButton = result.findViewById(R.id.confirm_button);
            confirmButton.setOnClickListener(view -> confirmSettings());
        }
        return result;
    }

    @Override
    public void onStart() {
        super.onStart();
        mProfileSyncService.addSyncStateChangedListener(this);

        if (!mIsFromSigninScreen
                || IdentityServicesProvider.get()
                           .getIdentityManager(getProfile())
                           .hasPrimaryAccount()) {
            return;
        }

        // Don't show CancelSyncDialog and hide bottom bar.
        mIsFromSigninScreen = false;
        View bottomBarShadow = getView().findViewById(R.id.bottom_bar_shadow);
        bottomBarShadow.setVisibility(View.GONE);
        View bottomBarButtonContainer = getView().findViewById(R.id.bottom_bar_button_container);
        bottomBarButtonContainer.setVisibility(View.GONE);

        ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
        assert actionBar != null;
        // Content description was overridden in onCreate, reset it to the standard one.
        actionBar.setHomeActionContentDescription(null);
    }

    @Override
    public void onStop() {
        super.onStop();

        mProfileSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_SYNC_REQUESTED.equals(key)) {
            assert canDisableSync();
            SyncSettingsUtils.enableSync((boolean) newValue);
            if (wasSigninFlowInterrupted()) {
                // This flow should only be reached when user toggles sync on.
                assert (boolean) newValue;
                mProfileSyncService.setFirstSetupComplete(
                        SyncFirstSetupCompleteSource.ADVANCED_FLOW_INTERRUPTED_TURN_SYNC_ON);
            }
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updatePreferences);
        } else if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            mPrefService.setBoolean(Pref.SEARCH_SUGGEST_ENABLED, (boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            mPrefService.setBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED, (boolean) newValue);
        } else if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
            UmaSessionStats.changeMetricsReportingConsent((boolean) newValue);
        } else if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    getProfile(), (boolean) newValue);
        } else if (PREF_AUTOFILL_ASSISTANT.equals(key)) {
            setAutofillAssistantSwitchValue((boolean) newValue);
        }
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
        updatePreferences();
    }

    /** Returns whether Sync can be disabled. */
    private boolean canDisableSync() {
        return !getProfile().isChild();
    }

    /** Returns whether user did not complete the sign in flow. */
    private boolean wasSigninFlowInterrupted() {
        return !mIsFromSigninScreen && !mProfileSyncService.isFirstSetupComplete();
    }

    private void displayPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this).show(ft, FRAGMENT_ENTER_PASSPHRASE);
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
        updatePreferences();
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

    @SyncError
    private int getSyncError() {
        @SyncError
        int error = SyncSettingsUtils.getSyncError();
        if (error == SyncError.SYNC_SETUP_INCOMPLETE && mIsFromSigninScreen) {
            return SyncError.NO_ERROR;
        }
        return error;
    }

    /**
     * Gets title message for sync error.
     * @param error The sync error.
     */
    private String getSyncErrorTitle(@SyncError int error) {
        switch (error) {
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return getString(R.string.sync_settings_not_confirmed_title);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return getString(R.string.sync_error_card_title);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return getString(R.string.password_sync_error_summary);
            default:
                return getString(R.string.sync_error_card_title);
        }
    }

    private void onSyncErrorCardClicked() {
        if (mCurrentSyncError == SyncError.NO_ERROR) {
            return;
        }

        if (mCurrentSyncError == SyncError.ANDROID_SYNC_DISABLED) {
            IntentUtils.safeStartActivity(getActivity(), new Intent(Settings.ACTION_SYNC_SETTINGS));
            return;
        }

        if (mCurrentSyncError == SyncError.AUTH_ERROR) {
            AccountManagerFacadeProvider.getInstance().updateCredentials(
                    CoreAccountInfo.getAndroidAccountFrom(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(getProfile())
                                    .getPrimaryAccountInfo(ConsentLevel.SYNC)),
                    getActivity(), null);
            return;
        }

        if (mCurrentSyncError == SyncError.CLIENT_OUT_OF_DATE) {
            // Opens the client in play store for update.
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse("market://details?id="
                    + ContextUtils.getApplicationContext().getPackageName()));
            startActivity(intent);
            return;
        }

        if (mCurrentSyncError == SyncError.OTHER_ERRORS) {
            final CoreAccountInfo account = IdentityServicesProvider.get()
                                                    .getIdentityManager(getProfile())
                                                    .getPrimaryAccountInfo(ConsentLevel.SYNC);
            // TODO(https://crbug.com/873116): Pass the correct reason for the signout.
            IdentityServicesProvider.get()
                    .getSigninManager(getProfile())
                    .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                            ()
                                    -> IdentityServicesProvider.get()
                                               .getSigninManager(getProfile())
                                               .signinAndEnableSync(
                                                       SigninAccessPoint.SYNC_ERROR_CARD, account,
                                                       null),
                            false);
            return;
        }

        if (mCurrentSyncError == SyncError.PASSPHRASE_REQUIRED) {
            displayPassphraseDialog();
            return;
        }

        if (mCurrentSyncError == SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING
                || mCurrentSyncError == SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS) {
            CoreAccountInfo primaryAccountInfo = IdentityServicesProvider.get()
                                                         .getIdentityManager(getProfile())
                                                         .getPrimaryAccountInfo(ConsentLevel.SYNC);
            if (primaryAccountInfo != null) {
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
            }
            return;
        }
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        updateSyncPreferences();

        mSearchSuggestions.setChecked(mPrefService.getBoolean(Pref.SEARCH_SUGGEST_ENABLED));
        mNavigationError.setChecked(mPrefService.getBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED));

        mUsageAndCrashReporting.setChecked(
                mPrivacyPrefManager.isUsageAndCrashReportingPermittedByUser());
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                        getProfile()));

        if (mAutofillAssistant != null) {
            mAutofillAssistant.setChecked(isAutofillAssistantSwitchOn());
        }
        if (mContextualSearch != null) {
            boolean isContextualSearchEnabled =
                    !ContextualSearchManager.isContextualSearchDisabled();
            mContextualSearch.setSummary(
                    isContextualSearchEnabled ? R.string.text_on : R.string.text_off);
        }
    }

    private void updateSyncPreferences() {
        if (!mProfileSyncService.isEngineInitialized()
                || !mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }

        if (!IdentityServicesProvider.get().getIdentityManager(getProfile()).hasPrimaryAccount()) {
            getPreferenceScreen().removePreference(mManageYourGoogleAccount);
            getPreferenceScreen().removePreference(mSyncCategory);
            return;
        }

        getPreferenceScreen().addPreference(mManageYourGoogleAccount);
        getPreferenceScreen().addPreference(mSyncCategory);
        if (ProfileSyncService.get().isSyncDisabledByEnterprisePolicy()) {
            mSyncCategory.addPreference(mSyncDisabledByAdministrator);
            mSyncCategory.removePreference(mSyncErrorCard);
            mSyncCategory.removePreference(mSyncRequested);
            mSyncCategory.removePreference(mManageSync);
            return;
        }
        mSyncCategory.removePreference(mSyncDisabledByAdministrator);
        mSyncCategory.addPreference(mSyncRequested);
        mSyncCategory.addPreference(mManageSync);

        mCurrentSyncError = getSyncError();
        if (mCurrentSyncError == SyncError.NO_ERROR) {
            mSyncCategory.removePreference(mSyncErrorCard);
        } else {
            mSyncErrorCard.setTitle(getSyncErrorTitle(mCurrentSyncError));
            mSyncErrorCard.setSummary(
                    SyncSettingsUtils.getSyncErrorHint(getActivity(), mCurrentSyncError));
            mSyncCategory.addPreference(mSyncErrorCard);
        }

        mSyncRequested.setChecked(AndroidSyncSettings.get().isChromeSyncEnabled());
        if (wasSigninFlowInterrupted()) {
            // If sync setup was not completed the sync request toggle should be off.
            // In this situation, switching it on will trigger a call to setFirstSetupComplete.
            mSyncRequested.setChecked(false);
        }
        mSyncRequested.setEnabled(canDisableSync());
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return mPrefService.isManagedPreference(Pref.ALTERNATE_ERROR_PAGES_ENABLED);
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return mPrefService.isManagedPreference(Pref.SEARCH_SUGGEST_ENABLED);
            }
            if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
                return PrivacyPreferencesManagerImpl.getInstance().isMetricsReportingManaged();
            }
            if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
                return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(
                        getProfile());
            }
            return false;
        };
    }

    @Override
    public boolean onBackPressed() {
        if (!mIsFromSigninScreen) return false; // Let parent activity handle it.
        showCancelSyncDialog();
        return true;
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
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram(getProfile());
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    /**
     * The dialog that offers the user to cancel sync. Only shown when
     * {@link SyncAndServicesSettings} is opened from the sign-in screen. Shown when the user
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
            SyncAndServicesSettings fragment = (SyncAndServicesSettings) getTargetFragment();
            fragment.cancelSync();
        }
    }

    /**
     *  This checks whether Autofill Assistant is enabled and was shown at least once (only then
     *  will the AA switch be assigned a value).
     */
    private boolean shouldShowAutofillAssistantPreference() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED);
    }

    public boolean isAutofillAssistantSwitchOn() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false);
    }

    public void setAutofillAssistantSwitchValue(boolean newValue) {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }

    private static Profile getProfile() {
        return Profile.getLastUsedRegularProfile();
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
}
