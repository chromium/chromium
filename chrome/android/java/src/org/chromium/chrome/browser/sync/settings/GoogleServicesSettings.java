// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.ChangeMetricsReportingStateCalledFrom;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.account_storage_toggle.AccountStorageToggleFragmentArgs;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SignOutCoordinator;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Settings fragment controlling a number of features communicating with Google services, such as
 * search autocomplete and the automatic upload of crash reports.
 */
public class GoogleServicesSettings extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceChangeListener,
                SyncStateChangedListener,
                ProfileDataCache.Observer {
    // No longer used. Do not delete. Do not reuse these same strings.
    // private static final String SIGN_OUT_DIALOG_TAG = "sign_out_dialog_tag";
    // public static final String PREF_AUTOFILL_ASSISTANT = "autofill_assistant";
    // public static final String PREF_AUTOFILL_ASSISTANT_SUBSECTION =
    // "autofill_assistant_subsection";

    @VisibleForTesting public static final String PREF_ALLOW_SIGNIN = "allow_signin";

    @VisibleForTesting
    public static final String PREF_PASSWORDS_ACCOUNT_STORAGE = "passwords_account_storage";

    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";

    @VisibleForTesting
    public static final String PREF_USAGE_STATS_REPORTING = "usage_stats_reporting";
    @VisibleForTesting
    public static final String PREF_PRICE_TRACKING_ANNOTATIONS = "price_tracking_annotations";

    private static final String PREF_PRICE_NOTIFICATION_SECTION = "price_notifications_section";

    private final PrivacyPreferencesManagerImpl mPrivacyPrefManager =
            PrivacyPreferencesManagerImpl.getInstance();

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private PrefService mPrefService;
    private ProfileDataCache mProfileDataCache;

    private ChromeSwitchPreference mAllowSignin;
    private ChromeSwitchPreference mPasswordsAccountStorage;
    private ChromeSwitchPreference mSearchSuggestions;
    private ChromeSwitchPreference mUsageAndCrashReporting;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;
    private ChromeSwitchPreference mPriceTrackingAnnotations;
    private @Nullable Preference mContextualSearch;
    private Preference mPriceNotificationSection;
    private Preference mUsageStatsReporting;
    private OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.prefs_google_services));
        setHasOptionsMenu(true);

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(getActivity());
        mProfileDataCache.addObserver(this);
        mPrefService = UserPrefs.get(getProfile());
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        SettingsUtils.addPreferencesFromResource(this, R.xml.google_services_preferences);

        mAllowSignin = (ChromeSwitchPreference) findPreference(PREF_ALLOW_SIGNIN);

        if (getProfile().isChild()) {
            // Do not display option to allow / disallow sign-in for supervised accounts since
            // these require the user to be signed-in and syncing.
            mAllowSignin.setVisible(false);
        } else {
            mAllowSignin.setOnPreferenceChangeListener(this);
            mAllowSignin.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        }

        mPasswordsAccountStorage =
                (ChromeSwitchPreference) findPreference(PREF_PASSWORDS_ACCOUNT_STORAGE);
        mPasswordsAccountStorage.setOnPreferenceChangeListener(this);
        if (getArguments() != null
                && getArguments().getBoolean(AccountStorageToggleFragmentArgs.HIGHLIGHT)) {
            mPasswordsAccountStorage.setBackgroundColor(
                    ChromeSemanticColorUtils.getIphHighlightColor(getContext()));
        }
        SyncServiceFactory.getForProfile(getProfile()).addSyncStateChangedListener(this);

        mSearchSuggestions = (ChromeSwitchPreference) findPreference(PREF_SEARCH_SUGGESTIONS);
        mSearchSuggestions.setOnPreferenceChangeListener(this);
        mSearchSuggestions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUsageAndCrashReporting =
                (ChromeSwitchPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        mUsageAndCrashReporting.setOnPreferenceChangeListener(this);
        mUsageAndCrashReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener(this);
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mContextualSearch = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (!ContextualSearchFieldTrial.isEnabled()) {
            removePreference(getPreferenceScreen(), mContextualSearch);
            mContextualSearch = null;
        }

        mPriceTrackingAnnotations =
                (ChromeSwitchPreference) findPreference(PREF_PRICE_TRACKING_ANNOTATIONS);
        if (!PriceTrackingFeatures.allowUsersToDisablePriceAnnotations(getProfile())) {
            removePreference(getPreferenceScreen(), mPriceTrackingAnnotations);
            mPriceTrackingAnnotations = null;
        } else {
            mPriceTrackingAnnotations.setOnPreferenceChangeListener(this);
            mPriceTrackingAnnotations.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        }

        mPriceNotificationSection = findPreference(PREF_PRICE_NOTIFICATION_SECTION);
        if (CommerceFeatureUtils.isShoppingListEligible(
                ShoppingServiceFactory.getForProfile(getProfile()))) {
            mPriceNotificationSection.setVisible(true);
        } else {
            removePreference(getPreferenceScreen(), mPriceNotificationSection);
            mPriceNotificationSection = null;
        }

        mUsageStatsReporting = findPreference(PREF_USAGE_STATS_REPORTING);
        mUsageStatsReporting.setVisible(true);

        updatePreferences();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        SyncServiceFactory.getForProfile(getProfile()).removeSyncStateChangedListener(this);
        mProfileDataCache.removeObserver(this);
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
                    .show(getActivity(), getString(R.string.help_context_sync_and_services), null);
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_ALLOW_SIGNIN.equals(key)) {
            assert !getProfile().isChild() : "A supervised account must not update allow sign-in.";

            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(getProfile());
            boolean shouldSignUserOut =
                    identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN) && !((boolean) newValue);
            if (!shouldSignUserOut) {
                mPrefService.setBoolean(Pref.SIGNIN_ALLOWED, (boolean) newValue);
                return true;
            }

            if (!ChromeFeatureList.isEnabled(
                            ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                    && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
                // Don't show signout dialog if there's no sync consent, as it never wipes the data.
                IdentityServicesProvider.get()
                        .getSigninManager(getProfile())
                        .signOut(SignoutReason.USER_DISABLED_ALLOW_CHROME_SIGN_IN, null, false);
                mPrefService.setBoolean(Pref.SIGNIN_ALLOWED, false);
                return true;
            }

            // TODO(crbug.com/350699437): Use a different SignoutReason.
            SignOutCoordinator.startSignOutFlow(
                    requireContext(),
                    getProfile(),
                    getActivity().getSupportFragmentManager(),
                    ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(),
                    mSnackbarManagerSupplier.get(),
                    SignoutReason.USER_DISABLED_ALLOW_CHROME_SIGN_IN,
                    /* showConfirmDialog= */ true,
                    () -> {
                        mPrefService.setBoolean(Pref.SIGNIN_ALLOWED, false);
                        updatePreferences();
                    });
            // Don't change the preference state yet, it will be updated by SignOutCoordinator if
            // the user actually confirms the sign-out.
            return false;
        } else if (PREF_PASSWORDS_ACCOUNT_STORAGE.equals(key)) {
            SyncServiceFactory.getForProfile(getProfile())
                    .setSelectedType(UserSelectableType.PASSWORDS, (boolean) newValue);
        } else if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            mPrefService.setBoolean(Pref.SEARCH_SUGGEST_ENABLED, (boolean) newValue);
        } else if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
            UmaSessionStats.changeMetricsReportingConsent(
                    (boolean) newValue, ChangeMetricsReportingStateCalledFrom.UI_SETTINGS);
        } else if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    getProfile(), (boolean) newValue);
        } else if (PREF_PRICE_TRACKING_ANNOTATIONS.equals(key)) {
            PriceTrackingUtilities.setTrackPricesOnTabsEnabled((boolean) newValue);
        }
        return true;
    }

    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    // SyncStateChangedListener overrides.
    @Override
    public void syncStateChanged() {
        updatePasswordsAccountStoragePreference();
    }

    // ProfileDataCache.Observer overrides.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updatePasswordsAccountStoragePreference();
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        mAllowSignin.setChecked(mPrefService.getBoolean(Pref.SIGNIN_ALLOWED));
        updatePasswordsAccountStoragePreference();
        mSearchSuggestions.setChecked(mPrefService.getBoolean(Pref.SEARCH_SUGGEST_ENABLED));
        mUsageAndCrashReporting.setChecked(mPrivacyPrefManager.isUsageAndCrashReportingPermitted());
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                        getProfile()));

        if (mContextualSearch != null) {
            boolean isContextualSearchEnabled =
                    !ContextualSearchManager.isContextualSearchDisabled(getProfile());
            mContextualSearch.setSummary(
                    isContextualSearchEnabled ? R.string.text_on : R.string.text_off);
        }
        if (mPriceTrackingAnnotations != null) {
            mPriceTrackingAnnotations.setChecked(
                    PriceTrackingUtilities.isTrackPricesOnTabsEnabled(getProfile()));
        }
        if (mUsageStatsReporting != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)) {
                mUsageStatsReporting.setOnPreferenceClickListener(
                        preference -> {
                            UsageStatsConsentDialog.create(
                                            getActivity(),
                                            getProfile(),
                                            true,
                                            (didConfirm) -> {
                                                if (didConfirm) {
                                                    updatePreferences();
                                                }
                                            })
                                    .show();
                            return true;
                        });
            } else {
                removePreference(getPreferenceScreen(), mUsageStatsReporting);
                mUsageStatsReporting = null;
            }
        }
    }

    private void updatePasswordsAccountStoragePreference() {
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        mPasswordsAccountStorage.setChecked(
                syncService.getSelectedTypes().contains(UserSelectableType.PASSWORDS));
        CoreAccountInfo account = syncService.getAccountInfo();
        mPasswordsAccountStorage.setVisible(
                syncService.getAccountInfo() != null
                        && !syncService.hasSyncConsent()
                        && !PasswordManagerUtilBridge.isGmsCoreUpdateRequired(
                                UserPrefs.get(getProfile()), syncService)
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
                        && !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS));
        if (account == null) {
            // The toggle is not visible, no need to set a summary.
            return;
        }
        boolean canDisplayEmail =
                mProfileDataCache
                        .getProfileDataOrDefault(account.getEmail())
                        .hasDisplayableEmailAddress();
        mPasswordsAccountStorage.setSummary(
                canDisplayEmail
                        ? getString(
                                R.string.passwords_account_storage_toggle_summary,
                                syncService.getAccountInfo().getEmail())
                        : getString(R.string.passwords_account_storage_toggle_summary_no_email));
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                String key = preference.getKey();
                if (PREF_ALLOW_SIGNIN.equals(key)) {
                    return mPrefService.isManagedPreference(Pref.SIGNIN_ALLOWED);
                }
                if (PREF_PASSWORDS_ACCOUNT_STORAGE.equals(key)) {
                    return SyncServiceFactory.getForProfile(getProfile())
                            .isTypeManagedByPolicy(UserSelectableType.PASSWORDS);
                }
                if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                    return mPrefService.isManagedPreference(Pref.SEARCH_SUGGEST_ENABLED);
                }
                if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
                    return !PrivacyPreferencesManagerImpl.getInstance()
                            .isUsageAndCrashReportingPermittedByPolicy();
                }
                if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
                    return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(
                            getProfile());
                }
                return false;
            }
        };
    }
}
