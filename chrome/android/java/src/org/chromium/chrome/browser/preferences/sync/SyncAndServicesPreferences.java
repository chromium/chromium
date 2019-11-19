// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.sync;

import android.accounts.Account;
import android.app.Dialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceCategory;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceGroup;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Settings fragment to enable Sync and other services that communicate with Google.
 */
public class SyncAndServicesPreferences extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener, Preferences.OnBackPressedListener {
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
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_SAFE_BROWSING_SCOUT_REPORTING =
            "safe_browsing_scout_reporting";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";

    @IntDef({SyncError.NO_ERROR, SyncError.ANDROID_SYNC_DISABLED, SyncError.AUTH_ERROR,
            SyncError.PASSPHRASE_REQUIRED, SyncError.CLIENT_OUT_OF_DATE,
            SyncError.SYNC_SETUP_INCOMPLETE, SyncError.OTHER_ERRORS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SyncError {
        int NO_ERROR = -1;
        int ANDROID_SYNC_DISABLED = 0;
        int AUTH_ERROR = 1;
        int PASSPHRASE_REQUIRED = 2;
        int CLIENT_OUT_OF_DATE = 3;
        int SYNC_SETUP_INCOMPLETE = 4;
        int OTHER_ERRORS = 128;
    }

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();
    private final PrefServiceBridge mPrefServiceBridge = PrefServiceBridge.getInstance();
    private final PrivacyPreferencesManager mPrivacyPrefManager =
            PrivacyPreferencesManager.getInstance();
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();

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
    private ChromeSwitchPreference mSafeBrowsing;
    private ChromeSwitchPreference mSafeBrowsingReporting;
    private ChromeSwitchPreference mUsageAndCrashReporting;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;
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

        mPrivacyPrefManager.migrateNetworkPredictionPreferences();

        getActivity().setTitle(R.string.prefs_sync_and_services);
        setHasOptionsMenu(true);
        if (mIsFromSigninScreen) {
            ActionBar actionBar = ((AppCompatActivity) getActivity()).getSupportActionBar();
            assert actionBar != null;
            actionBar.setHomeActionContentDescription(
                    R.string.prefs_sync_and_services_content_description);
            RecordUserAction.record("Signin_Signin_ShowAdvancedSyncSettings");
        }

        PreferenceUtils.addPreferencesFromResource(this, R.xml.sync_and_services_preferences);

        mSigninPreference = (SignInPreference) findPreference(PREF_SIGNIN);
        mSigninPreference.setPersonalizedPromoEnabled(false);
        mManageYourGoogleAccount = findPreference(PREF_MANAGE_YOUR_GOOGLE_ACCOUNT);
        mManageYourGoogleAccount.setOnPreferenceClickListener(SyncPreferenceUtils.toOnClickListener(
                this, () -> SyncPreferenceUtils.openGoogleMyAccount(getActivity())));

        mSyncCategory = (PreferenceCategory) findPreference(PREF_SYNC_CATEGORY);
        mSyncErrorCard = findPreference(PREF_SYNC_ERROR_CARD);
        mSyncErrorCard.setIcon(UiUtils.getTintedDrawable(
                getActivity(), R.drawable.ic_sync_error_40dp, R.color.default_red));
        mSyncErrorCard.setOnPreferenceClickListener(
                SyncPreferenceUtils.toOnClickListener(this, this::onSyncErrorCardClicked));
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

        mSafeBrowsing = (ChromeSwitchPreference) findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsing.setOnPreferenceChangeListener(this);
        mSafeBrowsing.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsingReporting =
                (ChromeSwitchPreference) findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING);
        mSafeBrowsingReporting.setOnPreferenceChangeListener(this);
        mSafeBrowsingReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUsageAndCrashReporting =
                (ChromeSwitchPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        mUsageAndCrashReporting.setOnPreferenceChangeListener(this);
        mUsageAndCrashReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener(this);
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        PreferenceCategory servicesCategory =
                (PreferenceCategory) findPreference(PREF_SERVICES_CATEGORY);
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                && wasSigninFlowInterrupted()) {
            // If the setup flow was previously interrupted, and now the user dismissed the page
            // without turning sync on, then mark first setup as complete (so that we won't show the
            // error again), but turn sync off.
            assert !mSyncRequested.isChecked();
            SyncPreferenceUtils.enableSync(false);
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
            if (!mIsFromSigninScreen) return false; // Let Preferences activity handle it.
            showCancelSyncDialog();
            return true;
        } else if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services),
                    Profile.getLastUsedProfile(), null);
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
        mSigninPreference.registerForUpdates();

        if (!mIsFromSigninScreen || ChromeSigninController.get().isSignedIn()) {
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

        mSigninPreference.unregisterForUpdates();
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
            SyncPreferenceUtils.enableSync((boolean) newValue);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                    && wasSigninFlowInterrupted()) {
                // This flow should only be reached when user toggles sync on.
                assert (boolean) newValue;
                mProfileSyncService.setFirstSetupComplete(
                        SyncFirstSetupCompleteSource.ADVANCED_FLOW_INTERRUPTED_TURN_SYNC_ON);
            }
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updatePreferences);
        } else if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.SEARCH_SUGGEST_ENABLED, (boolean) newValue);
        } else if (PREF_SAFE_BROWSING.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.SAFE_BROWSING_ENABLED, (boolean) newValue);
        } else if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
            SafeBrowsingBridge.setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED, (boolean) newValue);
        } else if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
            UmaSessionStats.changeMetricsReportingConsent((boolean) newValue);
        } else if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    (boolean) newValue);
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
        return !Profile.getLastUsedProfile().isChild();
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
        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return SyncError.ANDROID_SYNC_DISABLED;
        }

        if (!AndroidSyncSettings.get().isChromeSyncEnabled()) {
            return SyncError.NO_ERROR;
        }

        if (mProfileSyncService.getAuthError()
                == GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS) {
            return SyncError.AUTH_ERROR;
        }

        if (mProfileSyncService.requiresClientUpgrade()) {
            return SyncError.CLIENT_OUT_OF_DATE;
        }

        if (mProfileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                || mProfileSyncService.hasUnrecoverableError()) {
            return SyncError.OTHER_ERRORS;
        }

        if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            return SyncError.PASSPHRASE_REQUIRED;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                && wasSigninFlowInterrupted()) {
            return SyncError.SYNC_SETUP_INCOMPLETE;
        }

        return SyncError.NO_ERROR;
    }

    /**
     * Gets title message for sync error.
     * @param error The sync error.
     */
    private String getSyncErrorTitle(@SyncError int error) {
        switch (error) {
            case SyncError.SYNC_SETUP_INCOMPLETE:
                assert ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID);
                return getString(R.string.sync_settings_not_confirmed_title);
            default:
                return getString(R.string.sync_error_card_title);
        }
    }

    /**
     * Gets hint message to resolve sync error.
     * @param error The sync error.
     */
    private String getSyncErrorHint(@SyncError int error) {
        switch (error) {
            case SyncError.ANDROID_SYNC_DISABLED:
                return getString(R.string.hint_android_sync_disabled);
            case SyncError.AUTH_ERROR:
                return getString(R.string.hint_sync_auth_error);
            case SyncError.CLIENT_OUT_OF_DATE:
                return getString(
                        R.string.hint_client_out_of_date, BuildInfo.getInstance().hostPackageLabel);
            case SyncError.OTHER_ERRORS:
                return getString(R.string.hint_other_sync_errors);
            case SyncError.PASSPHRASE_REQUIRED:
                return getString(R.string.hint_passphrase_required);
            case SyncError.SYNC_SETUP_INCOMPLETE:
                assert ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID);
                return getString(R.string.hint_sync_settings_not_confirmed_description);
            case SyncError.NO_ERROR:
            default:
                return null;
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
            AccountManagerFacade.get().updateCredentials(
                    ChromeSigninController.get().getSignedInUser(), getActivity(), null);
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
            final Account account = ChromeSigninController.get().getSignedInUser();
            // TODO(https://crbug.com/873116): Pass the correct reason for the signout.
            IdentityServicesProvider.getSigninManager().signOut(
                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                    () -> IdentityServicesProvider.getSigninManager().signIn(account, null), false);
            return;
        }

        if (mCurrentSyncError == SyncError.PASSPHRASE_REQUIRED) {
            displayPassphraseDialog();
            return;
        }
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        updateSyncPreferences();

        mSearchSuggestions.setChecked(mPrefServiceBridge.getBoolean(Pref.SEARCH_SUGGEST_ENABLED));
        mNavigationError.setChecked(
                mPrefServiceBridge.getBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED));
        mSafeBrowsing.setChecked(mPrefServiceBridge.getBoolean(Pref.SAFE_BROWSING_ENABLED));
        mSafeBrowsingReporting.setChecked(
                SafeBrowsingBridge.isSafeBrowsingExtendedReportingEnabled());
        mUsageAndCrashReporting.setChecked(
                mPrivacyPrefManager.isUsageAndCrashReportingPermittedByUser());
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled());

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

        if (!ChromeSigninController.get().isSignedIn()) {
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
            mSyncErrorCard.setSummary(getSyncErrorHint(mCurrentSyncError));
            mSyncCategory.addPreference(mSyncErrorCard);
        }

        mSyncRequested.setChecked(AndroidSyncSettings.get().isChromeSyncEnabled());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                && wasSigninFlowInterrupted()) {
            // If sync setup was not completed the sync request toggle should be off.
            // In this situation, switching it on will trigger a call to setFirstSetupComplete.
            mSyncRequested.setChecked(false);
        }
        mSyncRequested.setEnabled(canDisableSync());
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.ALTERNATE_ERROR_PAGES_ENABLED);
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.SEARCH_SUGGEST_ENABLED);
            }
            if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
                return SafeBrowsingBridge.isSafeBrowsingExtendedReportingManaged();
            }
            if (PREF_SAFE_BROWSING.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.SAFE_BROWSING_ENABLED);
            }
            if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
                return PrivacyPreferencesManager.getInstance().isMetricsReportingManaged();
            }
            if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
                return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged();
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)) {
            ProfileSyncService.get().setFirstSetupComplete(
                    SyncFirstSetupCompleteSource.ADVANCED_FLOW_CONFIRM);
        }
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram();
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        IdentityServicesProvider.getSigninManager().signOut(
                SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    /**
     * The dialog that offers the user to cancel sync. Only shown when
     * {@link SyncAndServicesPreferences} is opened from the sign-in screen. Shown when the user
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
            SyncAndServicesPreferences fragment = (SyncAndServicesPreferences) getTargetFragment();
            fragment.cancelSync();
        }
    }
}
