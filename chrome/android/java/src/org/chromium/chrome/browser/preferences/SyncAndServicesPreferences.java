// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.app.DialogFragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.provider.Settings;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v4.util.ArraySet;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsEnabledStateUtils;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SignoutReason;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.ProtocolErrorClientAction;
import org.chromium.components.sync.StopSource;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption) and other services that
 * communicate with Google.
 */
public class SyncAndServicesPreferences extends PreferenceFragment
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener {
    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";

    private static final String PREF_SIGNIN = "sign_in";
    private static final String PREF_SYNC_ERROR_CARD = "sync_error_card";
    private static final String PREF_SYNC_ERROR_CARD_DIVIDER = "sync_error_card_divider";
    private static final String PREF_USE_SYNC_AND_ALL_SERVICES = "use_sync_and_all_services";

    private static final String PREF_SYNC_AND_PERSONALIZATION = "sync_and_personalization";
    private static final String PREF_SYNC_AUTOFILL = "sync_autofill";
    private static final String PREF_SYNC_BOOKMARKS = "sync_bookmarks";
    private static final String PREF_SYNC_PAYMENTS_INTEGRATION = "sync_payments_integration";
    private static final String PREF_SYNC_HISTORY = "sync_history";
    private static final String PREF_SYNC_PASSWORDS = "sync_passwords";
    private static final String PREF_SYNC_RECENT_TABS = "sync_recent_tabs";
    private static final String PREF_SYNC_SETTINGS = "sync_settings";
    private static final String PREF_SYNC_ACTIVITY_AND_INTERACTIONS =
            "sync_activity_and_interactions";
    private static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";
    private static final String PREF_ENCRYPTION = "encryption";
    private static final String PREF_SYNC_MANAGE_DATA = "sync_manage_data";
    private static final String PREF_CONTEXTUAL_SUGGESTIONS = "contextual_suggestions";

    private static final String PREF_NONPERSONALIZED_SERVICES = "nonpersonalized_services";
    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_NETWORK_PREDICTIONS = "network_predictions";
    private static final String PREF_NAVIGATION_ERROR = "navigation_error";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_SAFE_BROWSING_SCOUT_REPORTING =
            "safe_browsing_scout_reporting";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";

    @IntDef({SyncError.NO_ERROR, SyncError.ANDROID_SYNC_DISABLED, SyncError.AUTH_ERROR,
            SyncError.PASSPHRASE_REQUIRED, SyncError.CLIENT_OUT_OF_DATE, SyncError.OTHER_ERRORS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SyncError {
        int NO_ERROR = -1;
        int ANDROID_SYNC_DISABLED = 0;
        int AUTH_ERROR = 1;
        int PASSPHRASE_REQUIRED = 2;
        int CLIENT_OUT_OF_DATE = 3;
        int OTHER_ERRORS = 128;
    }

    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();
    private final PrefServiceBridge mPrefServiceBridge = PrefServiceBridge.getInstance();
    private final PrivacyPreferencesManager mPrivacyPrefManager =
            PrivacyPreferencesManager.getInstance();
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();

    private SignInPreference mSigninPreference;
    private Preference mSyncErrorCard;
    private Preference mSyncErrorCardDivider;
    private ChromeSwitchPreference mUseSyncAndAllServices;

    private SigninExpandablePreferenceGroup mSyncGroup;
    private CheckBoxPreference mSyncAutofill;
    private CheckBoxPreference mSyncBookmarks;
    private CheckBoxPreference mSyncPaymentsIntegration;
    private CheckBoxPreference mSyncHistory;
    private CheckBoxPreference mSyncPasswords;
    private CheckBoxPreference mSyncRecentTabs;
    private CheckBoxPreference mSyncSettings;
    private CheckBoxPreference mSyncActivityAndInteractions;
    // Contains preferences for all sync data types.
    private CheckBoxPreference[] mSyncAllTypes;

    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;
    private @Nullable Preference mContextualSuggestions;

    private SigninExpandablePreferenceGroup mNonpersonalizedServices;
    private ChromeBaseCheckBoxPreference mSearchSuggestions;
    private ChromeBaseCheckBoxPreference mNetworkPredictions;
    private ChromeBaseCheckBoxPreference mNavigationError;
    private ChromeBaseCheckBoxPreference mSafeBrowsing;
    private ChromeBaseCheckBoxPreference mSafeBrowsingReporting;
    private ChromeBaseCheckBoxPreference mUsageAndCrashReporting;
    private ChromeBaseCheckBoxPreference mUrlKeyedAnonymizedData;
    private @Nullable Preference mContextualSearch;

    private boolean mIsEngineInitialized;
    private boolean mIsPassphraseRequired;
    /**
     * This is usually equal to AndroidSyncSettings.isSyncEnabled(), but may have a different value
     * if passphrase dialog is shown (see {@link #onStop} for details).
     */
    private boolean mIsSyncEnabled;
    private @SyncError int mCurrentSyncError = SyncError.NO_ERROR;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mPrivacyPrefManager.migrateNetworkPredictionPreferences();

        getActivity().setTitle(R.string.prefs_sync_and_services);
        setHasOptionsMenu(true);

        PreferenceUtils.addPreferencesFromResource(this, R.xml.sync_and_services_preferences);

        mSigninPreference = (SignInPreference) findPreference(PREF_SIGNIN);
        mSigninPreference.setPersonalizedPromoEnabled(false);

        mSyncErrorCard = findPreference(PREF_SYNC_ERROR_CARD);
        mSyncErrorCard.setOnPreferenceClickListener(
                toOnClickListener(this::onSyncErrorCardClicked));
        mSyncErrorCardDivider = findPreference(PREF_SYNC_ERROR_CARD_DIVIDER);

        mUseSyncAndAllServices =
                (ChromeSwitchPreference) findPreference(PREF_USE_SYNC_AND_ALL_SERVICES);
        mUseSyncAndAllServices.setOnPreferenceChangeListener(this);
        mSyncGroup =
                (SigninExpandablePreferenceGroup) findPreference(PREF_SYNC_AND_PERSONALIZATION);
        mNonpersonalizedServices =
                (SigninExpandablePreferenceGroup) findPreference(PREF_NONPERSONALIZED_SERVICES);

        mSyncAutofill = (CheckBoxPreference) findPreference(PREF_SYNC_AUTOFILL);
        mSyncBookmarks = (CheckBoxPreference) findPreference(PREF_SYNC_BOOKMARKS);
        mSyncPaymentsIntegration =
                (CheckBoxPreference) findPreference(PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncHistory = (CheckBoxPreference) findPreference(PREF_SYNC_HISTORY);
        mSyncPasswords = (CheckBoxPreference) findPreference(PREF_SYNC_PASSWORDS);
        mSyncRecentTabs = (CheckBoxPreference) findPreference(PREF_SYNC_RECENT_TABS);
        mSyncSettings = (CheckBoxPreference) findPreference(PREF_SYNC_SETTINGS);
        mSyncActivityAndInteractions =
                (CheckBoxPreference) findPreference(PREF_SYNC_ACTIVITY_AND_INTERACTIONS);

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                toOnClickListener(this::onSyncEncryptionClicked));
        mManageSyncData = findPreference(PREF_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(
                toOnClickListener(this::openDashboardTabInNewActivityStack));

        mContextualSuggestions = findPreference(PREF_CONTEXTUAL_SUGGESTIONS);
        if (!FeatureUtilities.areContextualSuggestionsEnabled(getActivity())
                || !ContextualSuggestionsEnabledStateUtils.shouldShowSettings()) {
            removePreference(mSyncGroup, mContextualSuggestions);
            mContextualSuggestions = null;
        }

        mSyncAllTypes = new CheckBoxPreference[] {mSyncAutofill, mSyncBookmarks,
                mSyncPaymentsIntegration, mSyncHistory, mSyncPasswords, mSyncRecentTabs,
                mSyncSettings, mSyncActivityAndInteractions};
        for (CheckBoxPreference type : mSyncAllTypes) {
            type.setOnPreferenceChangeListener(this);
        }

        mSearchSuggestions = (ChromeBaseCheckBoxPreference) findPreference(PREF_SEARCH_SUGGESTIONS);
        mSearchSuggestions.setOnPreferenceChangeListener(this);
        mSearchSuggestions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mNetworkPredictions =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_NETWORK_PREDICTIONS);
        mNetworkPredictions.setOnPreferenceChangeListener(this);
        mNetworkPredictions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mNavigationError = (ChromeBaseCheckBoxPreference) findPreference(PREF_NAVIGATION_ERROR);
        mNavigationError.setOnPreferenceChangeListener(this);
        mNavigationError.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsing = (ChromeBaseCheckBoxPreference) findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsing.setOnPreferenceChangeListener(this);
        mSafeBrowsing.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsingReporting =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING);
        mSafeBrowsingReporting.setOnPreferenceChangeListener(this);
        mSafeBrowsingReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUsageAndCrashReporting =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        mUsageAndCrashReporting.setOnPreferenceChangeListener(this);
        mUsageAndCrashReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUrlKeyedAnonymizedData =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener(this);
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mContextualSearch = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (!ContextualSearchFieldTrial.isEnabled()) {
            removePreference(mNonpersonalizedServices, mContextualSearch);
            mContextualSearch = null;
        }

        if (Profile.getLastUsedProfile().isChild()) {
            mGoogleActivityControls.setSummary(
                    R.string.sign_in_google_activity_controls_summary_child_account);
        }

        boolean useSyncAndAllServices = UnifiedConsentServiceBridge.isUnifiedConsentGiven();
        mSyncGroup.setExpanded(!useSyncAndAllServices);
        mNonpersonalizedServices.setExpanded(!useSyncAndAllServices);

        updatePreferences();
    }

    private Preference.OnPreferenceClickListener toOnClickListener(Runnable runnable) {
        return preference -> {
            if (!isResumed()) {
                // This event could come in after onPause if the user clicks back and the preference
                // at roughly the same time. See http://b/5983282.
                return false;
            }
            runnable.run();
            return false;
        };
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        ListView list = (ListView) getView().findViewById(android.R.id.list);
        list.setDivider(null);
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
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_sync_and_services),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void onStart() {
        super.onStart();
        mIsEngineInitialized = mProfileSyncService.isEngineInitialized();
        mIsPassphraseRequired =
                mIsEngineInitialized && mProfileSyncService.isPassphraseRequiredForDecryption();
        // This prevents sync from actually syncing until the dialog is closed.
        mProfileSyncService.setSetupInProgress(true);
        mProfileSyncService.addSyncStateChangedListener(this);
        updateSyncStateFromAndroidSyncSettings();

        mSigninPreference.registerForUpdates();
    }

    @Override
    public void onStop() {
        super.onStop();

        mProfileSyncService.removeSyncStateChangedListener(this);
        // If this activity is closing, apply configuration changes and tell sync that
        // the user is done configuring sync.
        if (getActivity().isChangingConfigurations()) return;
        // Only save state if internal and external state match. If a stop and clear comes
        // while the dialog is open, this will be false and settings won't be saved.
        if (mIsSyncEnabled && AndroidSyncSettings.isSyncEnabled()) {
            // Save the new data type state.
            configureSyncDataTypes();
            // Inform sync that the user has finished setting up sync at least once.
            mProfileSyncService.setFirstSetupComplete();
        }
        PersonalDataManager.setPaymentsIntegrationEnabled(mSyncPaymentsIntegration.isChecked());
        // Setup is done. This was preventing sync from turning on even if it was enabled.
        // TODO(crbug/557784): This needs to be set only when we think the user is done with
        // setting up. This means: 1) If the user leaves the Sync Settings screen (via back)
        // or, 2) If the user leaves the screen by tapping on "Manage Synced Data"
        mProfileSyncService.setSetupInProgress(false);

        mSigninPreference.unregisterForUpdates();
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_USE_SYNC_AND_ALL_SERVICES.equals(key)) {
            boolean enabled = (boolean) newValue;
            if (!enabled) {
                mSyncGroup.setExpanded(true);
                mNonpersonalizedServices.setExpanded(true);
            }
            UnifiedConsentServiceBridge.setUnifiedConsentGiven(enabled);
            ThreadUtils.postOnUiThread(this::updateSyncStateFromSelectedModelTypes);
            ThreadUtils.postOnUiThread(this::updateDataTypeState);
            ThreadUtils.postOnUiThread(this::updatePreferences);
        } else if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            mPrefServiceBridge.setSearchSuggestEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING.equals(key)) {
            mPrefServiceBridge.setSafeBrowsingEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
            mPrefServiceBridge.setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            mPrefServiceBridge.setNetworkPredictionEnabled((boolean) newValue);
            recordNetworkPredictionEnablingUMA((boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            mPrefServiceBridge.setResolveNavigationErrorEnabled((boolean) newValue);
        } else if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
            UmaSessionStats.changeMetricsReportingConsent((boolean) newValue);
        } else if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    (boolean) newValue);
        } else if (isSyncTypePreference(preference)) {
            ThreadUtils.postOnUiThread(() -> {
                boolean preferenceChecked = (boolean) newValue;
                if (preference == mSyncAutofill) {
                    // If the user checks the autofill sync checkbox, then enable and check the
                    // payments integration checkbox.
                    //
                    // If the user unchecks the autofill sync checkbox, then disable and uncheck
                    // the payments integration checkbox.
                    mSyncPaymentsIntegration.setEnabled(preferenceChecked);
                    mSyncPaymentsIntegration.setChecked(preferenceChecked);
                } else if (preference == mSyncHistory) {
                    // Disable 'Activity and interactions' if history is disabled, enable otherwise.
                    mSyncActivityAndInteractions.setEnabled(preferenceChecked);
                    mSyncActivityAndInteractions.setChecked(preferenceChecked);
                }
                updateSyncStateFromSelectedModelTypes();
            });
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
        boolean wasSyncInitialized = mIsEngineInitialized;
        boolean wasPassphraseRequired = mIsPassphraseRequired;
        mIsEngineInitialized = mProfileSyncService.isEngineInitialized();
        mIsPassphraseRequired =
                mIsEngineInitialized && mProfileSyncService.isPassphraseRequiredForDecryption();
        if (mIsEngineInitialized != wasSyncInitialized
                || mIsPassphraseRequired != wasPassphraseRequired) {
            // Update all because Password syncability is also affected by the engine.
            updateSyncPreferences();
        } else {
            updateSyncErrorCard();
        }
    }

    /** Returns whether Sync can be disabled. */
    private boolean canDisableSync() {
        return !Profile.getLastUsedProfile().isChild();
    }

    private boolean isSyncTypePreference(Preference preference) {
        for (Preference pref : mSyncAllTypes) {
            if (pref == preference) return true;
        }
        return false;
    }

    /**
     * Update the state of all settings from sync.
     *
     * This sets the state of the sync switch from external sync state and then calls
     * updateSyncPreferences, which uses that as its source of truth.
     */
    private void updateSyncStateFromAndroidSyncSettings() {
        mIsSyncEnabled = AndroidSyncSettings.isSyncEnabled();
        updateSyncPreferences();
    }

    /** Update sync preferences using mIsSyncEnabled to determine if sync is enabled. */
    private void updateSyncPreferences() {
        updateDataTypeState();
        updateEncryptionState();
        updateSyncErrorCard();
    }

    /** Enables sync if any of the data types is selected, otherwise disables sync. */
    private void updateSyncStateFromSelectedModelTypes() {
        boolean shouldEnableSync = UnifiedConsentServiceBridge.isUnifiedConsentGiven()
                || !getSelectedModelTypes().isEmpty() || !canDisableSync();
        if (mIsSyncEnabled == shouldEnableSync) return;
        mIsSyncEnabled = shouldEnableSync;
        if (shouldEnableSync) {
            mProfileSyncService.requestStart();
        } else {
            stopSync();
        }
        updateSyncPreferences();
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
        mSyncEncryption.setEnabled(mIsSyncEnabled && mIsEngineInitialized);
        mSyncEncryption.setSummary(null);
        if (!mIsEngineInitialized) {
            // If sync is not initialized, encryption state is unavailable and can't be changed.
            // Leave the button disabled and the summary empty. Additionally, close the dialogs in
            // case they were open when a stop and clear comes.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            return;
        }
        if (!mProfileSyncService.isPassphraseRequiredForDecryption()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mProfileSyncService.isPassphraseRequiredForDecryption() && isAdded()) {
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

    private void configureSyncDataTypes() {
        updateSyncStateFromSelectedModelTypes();
        if (!mIsSyncEnabled) return;

        boolean syncEverything = UnifiedConsentServiceBridge.isUnifiedConsentGiven();
        mProfileSyncService.setPreferredDataTypes(syncEverything, getSelectedModelTypes());
        // Update the invalidation listener with the set of types we are enabling.
        InvalidationController invController = InvalidationController.get();
        invController.ensureStartedAndUpdateRegisteredTypes();
    }

    private Set<Integer> getSelectedModelTypes() {
        Set<Integer> types = new HashSet<>();
        if (mSyncAutofill.isChecked()) types.add(ModelType.AUTOFILL);
        if (mSyncBookmarks.isChecked()) types.add(ModelType.BOOKMARKS);
        if (mSyncHistory.isChecked()) types.add(ModelType.TYPED_URLS);
        if (mSyncPasswords.isChecked()) types.add(ModelType.PASSWORDS);
        if (mSyncRecentTabs.isChecked()) types.add(ModelType.PROXY_TABS);
        if (mSyncSettings.isChecked()) types.add(ModelType.PREFERENCES);
        if (mSyncActivityAndInteractions.isChecked()) types.add(ModelType.USER_EVENTS);
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
        updateSyncStateFromAndroidSyncSettings();
        return true;
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!mProfileSyncService.isEngineInitialized()
                || !mProfileSyncService.isPassphraseRequiredForDecryption()) {
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
        // Configure the current set of data types - this tells the sync engine to
        // apply our encryption configuration changes.
        configureSyncDataTypes();
        // Re-display our config UI to properly reflect the new state.
        updateSyncStateFromAndroidSyncSettings();
    }

    /** Callback for PassphraseTypeDialogFragment.Listener */
    @Override
    public void onPassphraseTypeSelected(PassphraseType type) {
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

        if (mProfileSyncService.isPassphraseRequiredForDecryption()) {
            displayPassphraseDialog();
        } else {
            displayPassphraseTypeDialog();
        }
    }

    /** Opens the Google Dashboard where the user can control the data stored for the account. */
    private void openDashboardTabInNewActivityStack() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DASHBOARD_URL));
        intent.setPackage(getActivity().getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }

    /**
     * Update the data type switch state.
     *
     * If sync is on, load the prefs from native. Otherwise, all data types are disabled and
     * checked. Note that the Password data type will be shown as disabled and unchecked between
     * sync being turned on and the engine initialization completing.
     */
    private void updateDataTypeState() {
        boolean syncEverything = UnifiedConsentServiceBridge.isUnifiedConsentGiven();
        if (syncEverything) {
            for (CheckBoxPreference pref : mSyncAllTypes) {
                pref.setChecked(true);
                pref.setEnabled(false);
            }
            return;
        }

        Set<Integer> syncTypes =
                mIsSyncEnabled ? mProfileSyncService.getPreferredDataTypes() : new ArraySet<>();
        mSyncAutofill.setChecked(syncTypes.contains(ModelType.AUTOFILL));
        mSyncAutofill.setEnabled(true);
        mSyncBookmarks.setChecked(syncTypes.contains(ModelType.BOOKMARKS));
        mSyncBookmarks.setEnabled(true);
        mSyncHistory.setChecked(syncTypes.contains(ModelType.TYPED_URLS));
        mSyncHistory.setEnabled(true);
        mSyncRecentTabs.setChecked(syncTypes.contains(ModelType.PROXY_TABS));
        mSyncRecentTabs.setEnabled(true);
        mSyncSettings.setChecked(syncTypes.contains(ModelType.PREFERENCES));
        mSyncSettings.setEnabled(true);

        // Payments integration requires AUTOFILL model type
        boolean syncAutofill = syncTypes.contains(ModelType.AUTOFILL);
        mSyncPaymentsIntegration.setChecked(
                syncAutofill && PersonalDataManager.isPaymentsIntegrationEnabled());
        mSyncPaymentsIntegration.setEnabled(syncAutofill);

        boolean passwordsConfigurable = mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isCryptographerReady();
        mSyncPasswords.setChecked(passwordsConfigurable && syncTypes.contains(ModelType.PASSWORDS));
        mSyncPasswords.setEnabled(passwordsConfigurable);

        // USER_EVENTS sync type doesn't work with custom passphrase and needs history sync
        boolean userEventsConfigurable =
                !hasCustomPassphrase() && syncTypes.contains(ModelType.TYPED_URLS);
        mSyncActivityAndInteractions.setChecked(
                userEventsConfigurable && syncTypes.contains(ModelType.USER_EVENTS));
        mSyncActivityAndInteractions.setEnabled(userEventsConfigurable);
    }

    private boolean hasCustomPassphrase() {
        return mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.getPassphraseType() == PassphraseType.CUSTOM_PASSPHRASE;
    }

    private void updateSyncErrorCard() {
        mCurrentSyncError = getSyncError();
        if (mCurrentSyncError == SyncError.NO_ERROR) {
            getPreferenceScreen().removePreference(mSyncErrorCard);
            getPreferenceScreen().removePreference(mSyncErrorCardDivider);
        } else {
            String summary = getSyncErrorHint(mCurrentSyncError);
            mSyncErrorCard.setSummary(summary);
            getPreferenceScreen().addPreference(mSyncErrorCard);
            getPreferenceScreen().addPreference(mSyncErrorCardDivider);
        }
    }

    @SyncError
    private int getSyncError() {
        if (!AndroidSyncSettings.isMasterSyncEnabled()) {
            return SyncError.ANDROID_SYNC_DISABLED;
        }

        if (!mIsSyncEnabled) {
            return SyncError.NO_ERROR;
        }

        if (mProfileSyncService.getAuthError()
                == GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS) {
            return SyncError.AUTH_ERROR;
        }

        if (mProfileSyncService.getProtocolErrorClientAction()
                == ProtocolErrorClientAction.UPGRADE_CLIENT) {
            return SyncError.CLIENT_OUT_OF_DATE;
        }

        if (mProfileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                || mProfileSyncService.hasUnrecoverableError()) {
            return SyncError.OTHER_ERRORS;
        }

        if (mProfileSyncService.isSyncActive()
                && mProfileSyncService.isPassphraseRequiredForDecryption()) {
            return SyncError.PASSPHRASE_REQUIRED;
        }

        return SyncError.NO_ERROR;
    }

    /**
     * Gets hint message to resolve sync error.
     * @param error The sync error.
     */
    private String getSyncErrorHint(@SyncError int error) {
        Resources res = getActivity().getResources();
        switch (error) {
            case SyncError.ANDROID_SYNC_DISABLED:
                return res.getString(R.string.hint_android_sync_disabled);
            case SyncError.AUTH_ERROR:
                return res.getString(R.string.hint_sync_auth_error);
            case SyncError.CLIENT_OUT_OF_DATE:
                return res.getString(
                        R.string.hint_client_out_of_date, BuildInfo.getInstance().hostPackageLabel);
            case SyncError.OTHER_ERRORS:
                return res.getString(R.string.hint_other_sync_errors);
            case SyncError.PASSPHRASE_REQUIRED:
                return res.getString(R.string.hint_passphrase_required);
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
            SigninManager.get().signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                    () -> SigninManager.get().signIn(account, null, null));
            return;
        }

        if (mCurrentSyncError == SyncError.PASSPHRASE_REQUIRED) {
            displayPassphraseDialog();
            return;
        }
    }

    private void stopSync() {
        if (mProfileSyncService.isSyncRequested()) {
            RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                    StopSource.CHROME_SYNC_SETTINGS, StopSource.STOP_SOURCE_LIMIT);
            mProfileSyncService.requestStop();
        }
    }

    private void recordNetworkPredictionEnablingUMA(boolean enabled) {
        // Report user turning on and off NetworkPrediction.
        RecordHistogram.recordBooleanHistogram("PrefService.NetworkPredictionEnabled", enabled);
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        boolean useSyncAndAllServices = UnifiedConsentServiceBridge.isUnifiedConsentGiven();
        String signedInAccountName = ChromeSigninController.get().getSignedInAccountName();
        if (signedInAccountName != null) {
            getPreferenceScreen().removePreference(mSigninPreference);
            getPreferenceScreen().addPreference(mUseSyncAndAllServices);

            mUseSyncAndAllServices.setChecked(useSyncAndAllServices);
            mUseSyncAndAllServices.setEnabled(!hasCustomPassphrase());
            mSyncGroup.setEnabled(true);

            mGoogleActivityControls.setOnPreferenceClickListener(
                    toOnClickListener(() -> onGoogleActivityControlsClicked(signedInAccountName)));
        } else {
            getPreferenceScreen().addPreference(mSigninPreference);
            getPreferenceScreen().removePreference(mUseSyncAndAllServices);
            mSyncGroup.setExpanded(false);
            mSyncGroup.setEnabled(false);
        }

        if (mContextualSuggestions != null) {
            mContextualSuggestions.setSummary(
                    ContextualSuggestionsEnabledStateUtils.getEnabledState()
                            ? R.string.text_on
                            : R.string.text_off);
        }

        mSearchSuggestions.setChecked(mPrefServiceBridge.isSearchSuggestEnabled());
        mSearchSuggestions.setEnabled(!useSyncAndAllServices);
        mNetworkPredictions.setChecked(mPrefServiceBridge.getNetworkPredictionEnabled());
        mNetworkPredictions.setEnabled(!useSyncAndAllServices);
        mNavigationError.setChecked(mPrefServiceBridge.isResolveNavigationErrorEnabled());
        mNavigationError.setEnabled(!useSyncAndAllServices);
        mSafeBrowsing.setChecked(mPrefServiceBridge.isSafeBrowsingEnabled());
        mSafeBrowsing.setEnabled(!useSyncAndAllServices);
        mSafeBrowsingReporting.setChecked(
                mPrefServiceBridge.isSafeBrowsingExtendedReportingEnabled());
        mSafeBrowsingReporting.setEnabled(!useSyncAndAllServices);
        mUsageAndCrashReporting.setChecked(
                mPrivacyPrefManager.isUsageAndCrashReportingPermittedByUser());
        mUsageAndCrashReporting.setEnabled(!useSyncAndAllServices);
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled());
        mUrlKeyedAnonymizedData.setEnabled(!useSyncAndAllServices);

        if (mContextualSearch != null) {
            boolean isContextualSearchEnabled = !mPrefServiceBridge.isContextualSearchDisabled();
            mContextualSearch.setSummary(
                    isContextualSearchEnabled ? R.string.text_on : R.string.text_off);
        }
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return mPrefServiceBridge.isResolveNavigationErrorManaged();
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return mPrefServiceBridge.isSearchSuggestManaged();
            }
            if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
                return mPrefServiceBridge.isSafeBrowsingExtendedReportingManaged();
            }
            if (PREF_SAFE_BROWSING.equals(key)) {
                return mPrefServiceBridge.isSafeBrowsingManaged();
            }
            if (PREF_NETWORK_PREDICTIONS.equals(key)) {
                return mPrefServiceBridge.isNetworkPredictionManaged();
            }
            if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
                return mPrefServiceBridge.isMetricsReportingManaged();
            }
            if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
                return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged();
            }
            return false;
        };
    }
}
