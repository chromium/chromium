// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.SettingsNavigationHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordAccessLossDialogHelper;
import org.chromium.chrome.browser.password_manager.PasswordExportLauncher;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.password_manager.settings.PasswordsPreference;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.settings_promo_card.SettingsPromoCardPreference;
import org.chromium.chrome.browser.ui.signin.SignOutCoordinator;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.HashMap;
import java.util.Map;

/** The main settings screen, shown when the user first opens Settings. */
public class MainSettings extends ChromeBaseSettingsFragment
        implements TemplateUrlService.LoadListener,
                SyncService.SyncStateChangedListener,
                SigninManager.SignInStateObserver,
                SettingsCustomTabLauncher.SettingsCustomTabLauncherClient {
    public static final String PREF_SETTINGS_PROMO_CARD = "settings_promo_card";
    public static final String PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION =
            "account_and_google_services_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_MANAGE_SYNC = "manage_sync";
    public static final String PREF_GOOGLE_SERVICES = "google_services";
    public static final String PREF_BASICS_SECTION = "basics_section";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_TABS = "tabs";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_HOME_MODULES_CONFIG = "home_modules_config";
    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";
    public static final String PREF_AUTOFILL_SECTION = "autofill_section";
    public static final String PREF_PRIVACY = "privacy";
    public static final String PREF_SAFETY_CHECK = "safety_check";
    public static final String PREF_NOTIFICATIONS = "notifications";
    public static final String PREF_DOWNLOADS = "downloads";
    public static final String PREF_DEVELOPER = "developer";
    public static final String PREF_AUTOFILL_OPTIONS = "autofill_options";
    public static final String PREF_AUTOFILL_ADDRESSES = "autofill_addresses";
    public static final String PREF_AUTOFILL_PAYMENTS = "autofill_payment_methods";
    public static final String PREF_PLUS_ADDRESSES = "plus_addresses";
    public static final String PREF_SAFETY_HUB = "safety_hub";
    public static final String PREF_ADDRESS_BAR = "address_bar";
    public static final String PREF_APPEARANCE = "appearance";
    @VisibleForTesting static final int NEW_LABEL_MAX_VIEW_COUNT = 6;

    private final Map<String, Preference> mAllPreferences = new HashMap<>();

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private ChromeBasePreference mManageSync;
    private @Nullable PasswordCheck mPasswordCheck;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    // TODO(crbug.com/343933167): This should be removed when the snackbar issue is addressed.
    // Will be true if `onSignedOut()` was called when the current activity state is not
    // `Lifecycle.State.STARTED`.
    private boolean mShouldShowSnackbar;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    public MainSettings() {
        setHasOptionsMenu(true);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        createPreferences();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPageTitle.set(getString(R.string.settings));
        mPasswordCheck = PasswordCheckFactory.getOrCreate();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            signinManager.addSignInStateObserver(this);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            signinManager.removeSignInStateObserver(this);
        }
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing() && mPasswordCheck != null) PasswordCheckFactory.destroy();
    }

    @Override
    public void onStart() {
        super.onStart();
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        if (mShouldShowSnackbar) {
            mShouldShowSnackbar = false;
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::showSignoutSnackbar);
        }
        updatePreferences();
    }

    @Override
    public void onStop() {
        super.onStop();
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Ensure the preference disabled state is reflected when device is folded or unfolded.
        updateAddressBarPreference();
    }

    @Override
    public void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher) {
        mSettingsCustomTabLauncher = customTabLauncher;
    }

    private void createPreferences() {
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        SettingsUtils.addPreferencesFromResource(
                this,
                useLegacySettingsOrder() ? R.xml.main_preferences_legacy : R.xml.main_preferences);

        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(getContext());
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)) {
            // TODO(crbug.com/364906215): Define SettingsPromoCardPreference in the xml once
            // SyncPromoPreference is removed.
            SettingsPromoCardPreference settingsPromoCardPreference =
                    new SettingsPromoCardPreference(
                            getContext(), null, TrackerFactory.getTrackerForProfile(getProfile()));
            settingsPromoCardPreference.setKey(PREF_SETTINGS_PROMO_CARD);
            settingsPromoCardPreference.setOrder(0);
            getPreferenceScreen().addPreference(settingsPromoCardPreference);
        }

        SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
        signInPreference.initialize(getProfile(), profileDataCache, accountManagerFacade);

        ChromeBasePreference googleServicePreference = findPreference(PREF_GOOGLE_SERVICES);
        googleServicePreference.setViewId(R.id.account_management_google_services_row);

        cachePreferences();
        updateAutofillPreferences();
        updatePlusAddressesPreference();

        // TODO(crbug.com/40242060): Remove the passwords managed subtitle for local and UPM
        // unenrolled users who can see it directly in the context of the setting.
        setManagedPreferenceDelegateForPreference(PREF_PASSWORDS);
        setManagedPreferenceDelegateForPreference(PREF_SEARCH_ENGINE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If we are on Android O+ the Notifications preference should lead to the Android
            // Settings notifications page.
            Intent intent = new Intent();
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(
                    Settings.EXTRA_APP_PACKAGE,
                    ContextUtils.getApplicationContext().getPackageName());
            PackageManager pm = getActivity().getPackageManager();
            if (intent.resolveActivity(pm) != null) {
                Preference notifications = findPreference(PREF_NOTIFICATIONS);
                notifications.setOnPreferenceClickListener(
                        preference -> {
                            startActivity(intent);
                            // We handle the click so the default action isn't triggered.
                            return true;
                        });
            } else {
                removePreferenceIfPresent(PREF_NOTIFICATIONS);
            }
        } else {
            // The per-website notification settings page can be accessed from Site
            // Settings, so we don't need to show this here.
            getPreferenceScreen().removePreference(findPreference(PREF_NOTIFICATIONS));
        }

        TemplateUrlService templateUrlService =
                TemplateUrlServiceFactory.getForProfile(getProfile());
        if (!templateUrlService.isLoaded()) {
            templateUrlService.registerLoadListener(this);
            templateUrlService.load();
        }

        if (!ChromeFeatureList.sAndroidAppearanceSettings.isEnabled()) {
            removePreferenceIfPresent(PREF_APPEARANCE);

            // LINT.IfChange(InitPrefToolbarShortcut)
            new AdaptiveToolbarStatePredictor(
                            getContext(),
                            getProfile(),
                            /* androidPermissionDelegate= */ null,
                            /* behavior= */ null)
                    .recomputeUiState(
                            uiState -> {
                                // Don't show toolbar shortcut settings if disabled from finch.
                                if (!uiState.canShowUi) {
                                    removePreferenceIfPresent(PREF_TOOLBAR_SHORTCUT);
                                }
                            });
            // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/appearance/settings/AppearanceSettingsFragment.java:InitPrefToolbarShortcut)

            // LINT.IfChange(InitPrefUiTheme)
            findPreference(PREF_UI_THEME)
                    .getExtras()
                    .putInt(
                            ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                            ThemeSettingsEntry.SETTINGS);
            // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/appearance/settings/AppearanceSettingsFragment.java:InitPrefUiTheme)
        } else {
            // NOTE: "Theme" and "Toolbar shortcut" move to "Appearance" settings when enabled.
            removePreferenceIfPresent(PREF_TOOLBAR_SHORTCUT);
            removePreferenceIfPresent(PREF_UI_THEME);
        }

        if (BuildInfo.getInstance().isAutomotive) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_HUB));
        } else if (!ChromeFeatureList.sSafetyHub.isEnabled()) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_HUB));
        } else {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
            findPreference(PREF_SAFETY_HUB)
                    .setOnPreferenceClickListener(
                            preference -> {
                                SafetyHubMetricUtils.recordExternalInteractions(
                                        SafetyHubMetricUtils.ExternalInteractions
                                                .OPEN_FROM_SETTINGS_PAGE);
                                return false;
                            });
        }
    }

    /**
     * Stores all preferences in memory so that, if they needed to be added/removed from the
     * PreferenceScreen, there would be no need to reload them from 'main_preferences.xml'.
     */
    private void cachePreferences() {
        int preferenceCount = getPreferenceScreen().getPreferenceCount();
        for (int index = 0; index < preferenceCount; index++) {
            Preference preference = getPreferenceScreen().getPreference(index);
            mAllPreferences.put(preference.getKey(), preference);
        }
        mManageSync = (ChromeBasePreference) findPreference(PREF_MANAGE_SYNC);
    }

    private void setManagedPreferenceDelegateForPreference(String key) {
        ChromeBasePreference chromeBasePreference = (ChromeBasePreference) mAllPreferences.get(key);
        chromeBasePreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
    }

    private void updatePreferences() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)) {
            SettingsPromoCardPreference promoCardPreference =
                    (SettingsPromoCardPreference) addPreferenceIfAbsent(PREF_SETTINGS_PROMO_CARD);
            promoCardPreference.updatePreferences();
        }

        if (IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .isSigninSupported(/* requireUpdatedPlayServices= */ false)) {
            addPreferenceIfAbsent(PREF_SIGN_IN);
        } else {
            removePreferenceIfPresent(PREF_SIGN_IN);
        }

        updateManageSyncPreference();
        updateSearchEnginePreference();
        updateAutofillPreferences();
        updatePlusAddressesPreference();
        updateAddressBarPreference();
        updateAppearancePreference();
        addPreferenceIfAbsent(PREF_TABS);

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.getInstance().isHomepageEnabled());

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION)
                && HomeModulesConfigManager.getInstance().hasModuleShownInSettings()) {
            addPreferenceIfAbsent(PREF_HOME_MODULES_CONFIG);
        } else {
            removePreferenceIfPresent(PREF_HOME_MODULES_CONFIG);
        }

        if (DeveloperSettings.shouldShowDeveloperSettings()) {
            addPreferenceIfAbsent(PREF_DEVELOPER);
        } else {
            removePreferenceIfPresent(PREF_DEVELOPER);
        }
    }

    private Preference addPreferenceIfAbsent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference == null) getPreferenceScreen().addPreference(mAllPreferences.get(key));
        return mAllPreferences.get(key);
    }

    private void removePreferenceIfPresent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    private void updateManageSyncPreference() {
        // TODO(crbug.com/40067770): Remove usage of ConsentLevel.SYNC after kSync users are
        // migrated to kSignin in phase 3. See ConsentLevel::kSync documentation for details.
        boolean isSyncConsentAvailable =
                IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null;
        mManageSync.setVisible(isSyncConsentAvailable);
        if (!isSyncConsentAvailable) return;

        mManageSync.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity(), getProfile()));
        mManageSync.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity(), getProfile()));

        mManageSync.setOnPreferenceClickListener(
                pref -> {
                    Context context = getContext();
                    if (SyncServiceFactory.getForProfile(getProfile())
                            .isSyncDisabledByEnterprisePolicy()) {
                        SyncSettingsUtils.showSyncDisabledByAdministratorToast(context);
                    } else {
                        SettingsNavigation settingsNavigation =
                                SettingsNavigationFactory.createSettingsNavigation();
                        settingsNavigation.startSettings(context, ManageSyncSettings.class);
                    }
                    return true;
                });
    }

    private void updateSearchEnginePreference() {
        TemplateUrlService templateUrlService =
                TemplateUrlServiceFactory.getForProfile(getProfile());
        if (!templateUrlService.isLoaded()) {
            ChromeBasePreference searchEnginePref =
                    (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
            searchEnginePref.setEnabled(false);
            return;
        }

        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();

        Preference searchEnginePreference = findPreference(PREF_SEARCH_ENGINE);
        searchEnginePreference.setEnabled(true);
        searchEnginePreference.setSummary(defaultSearchEngineName);
    }

    private void updateAutofillPreferences() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && ChromeFeatureList.isEnabled(
                        AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            addPreferenceIfAbsent(PREF_AUTOFILL_SECTION);
            addPreferenceIfAbsent(PREF_AUTOFILL_OPTIONS);
            Preference preference = findPreference(PREF_AUTOFILL_OPTIONS);
            preference.setFragment(null);
            preference.setOnPreferenceClickListener(
                    unused -> {
                        SettingsNavigationFactory.createSettingsNavigation()
                                .startSettings(
                                        getContext(),
                                        AutofillOptionsFragment.class,
                                        AutofillOptionsFragment.createRequiredArgs(
                                                AutofillOptionsReferrer.SETTINGS));
                        return true; // Means event is consumed.
                    });
        } else {
            removePreferenceIfPresent(PREF_AUTOFILL_SECTION);
            removePreferenceIfPresent(PREF_AUTOFILL_OPTIONS);
        }
        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsNavigationHelper.showAutofillCreditCardSettings(
                                        getActivity()));
        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(
                        preference ->
                                SettingsNavigationHelper.showAutofillProfileSettings(
                                        getActivity()));
        PasswordsPreference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setProfile(getProfile());
        passwordsPreference.setOnPreferenceClickListener(
                preference -> {
                    PasswordManagerLauncher.showPasswordSettings(
                            getActivity(),
                            getProfile(),
                            ManagePasswordsReferrer.CHROME_SETTINGS,
                            mModalDialogManagerSupplier,
                            /* managePasskeys= */ false);
                    return true;
                });

        // This is temporary code needed for migrating people to UPM. With UPM there is no
        // longer passwords setting page in Chrome, so we need to ask users to export their
        // passwords here, in main settings.
        boolean startPasswordsExportFlow =
                getArguments() != null
                        && getArguments().containsKey(PasswordExportLauncher.START_PASSWORDS_EXPORT)
                        && getArguments().getBoolean(PasswordExportLauncher.START_PASSWORDS_EXPORT);
        if (startPasswordsExportFlow) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)) {
                assert mSettingsCustomTabLauncher != null
                        : "The CSV download flow dialog requires a non-null"
                                + " SettingsCustomTabLauncher.";
                PasswordManagerHelper.getForProfile(getProfile())
                        .launchDownloadPasswordsCsvFlow(getContext(), mSettingsCustomTabLauncher);
            } else {
                PasswordAccessLossDialogHelper.launchExportFlow(
                        getContext(), getProfile(), mModalDialogManagerSupplier);
            }
            getArguments().putBoolean(PasswordExportLauncher.START_PASSWORDS_EXPORT, false);
        }
    }

    private void updatePlusAddressesPreference() {
        // TODO(crbug.com/40276862): Replace with a static string once name is finalized.
        String title =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PLUS_ADDRESSES_ENABLED, "settings-label");
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESSES_ENABLED)
                && !title.isEmpty()) {
            addPreferenceIfAbsent(PREF_PLUS_ADDRESSES);
            Preference preference = findPreference(PREF_PLUS_ADDRESSES);
            preference.setTitle(title);
            preference.setOnPreferenceClickListener(
                    unused -> {
                        String url =
                                ChromeFeatureList.getFieldTrialParamByFeature(
                                        ChromeFeatureList.PLUS_ADDRESSES_ENABLED, "manage-url");
                        CustomTabActivity.showInfoPage(getContext(), url);
                        return true;
                    });
        } else {
            removePreferenceIfPresent(PREF_PLUS_ADDRESSES);
        }
    }

    private void updateAddressBarPreference() {
        // Similar to ToolbarPositionController#isToolbarPositionCustomizationEnabled(), except
        // - no CCT checks (settings are not accessible from CCTs),
        // - showing on Foldables in unfolded (open) state.
        boolean showSetting =
                ChromeFeatureList.sAndroidBottomToolbar.isEnabled()
                        && !DeviceInfo.isAutomotive()
                        && (BuildInfo.getInstance().isFoldable
                                || !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                                        getContext()));

        if (showSetting) {
            Preference addressBarPreference = addPreferenceIfAbsent(PREF_ADDRESS_BAR);
            addressBarPreference.setSummary(ToolbarPositionController.getToolbarPositionResId());
            updateNewPreferenceAndIncrementViewCount(
                    addressBarPreference,
                    AddressBarSettingsFragment.getTitle(getContext()),
                    ChromePreferenceKeys.ADDRESS_BAR_SETTINGS_CLICKED,
                    ChromePreferenceKeys.ADDRESS_BAR_SETTINGS_VIEW_COUNT);
        } else {
            removePreferenceIfPresent(PREF_ADDRESS_BAR);
        }
    }

    private void updateAppearancePreference() {
        if (ChromeFeatureList.sAndroidAppearanceSettings.isEnabled()) {
            updateNewPreferenceAndIncrementViewCount(
                    findPreference(PREF_APPEARANCE),
                    AppearanceSettingsFragment.getTitle(getContext()),
                    ChromePreferenceKeys.APPEARANCE_SETTINGS_CLICKED,
                    ChromePreferenceKeys.APPEARANCE_SETTINGS_VIEW_COUNT);
        }
    }

    private void updateNewPreferenceAndIncrementViewCount(
            @NonNull Preference pref,
            @NonNull String title,
            @NonNull String clickedPrefKey,
            @NonNull String viewCountPrefKey) {
        final SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();

        boolean clicked;
        try {
            clicked = sharedPreferences.readBoolean(clickedPrefKey, false);
        } catch (ClassCastException e) {
            // Clean up pref value mis-written as int.
            sharedPreferences.writeBoolean(clickedPrefKey, true);
            clicked = true;
        }

        final int viewCount = sharedPreferences.readInt(viewCountPrefKey, 0);
        final boolean showNewLabelForPref = !clicked && viewCount < NEW_LABEL_MAX_VIEW_COUNT;

        if (!showNewLabelForPref) {
            pref.setTitle(title);
            pref.setOnPreferenceClickListener(null);
            return;
        }

        sharedPreferences.incrementInt(viewCountPrefKey);

        final Context context = getContext();
        pref.setTitle(
                SpanApplier.applySpans(
                        context.getString(R.string.prefs_new_label, title),
                        new SpanInfo(
                                "<new>",
                                "</new>",
                                new SuperscriptSpan(),
                                new RelativeSizeSpan(0.75f),
                                new ForegroundColorSpan(
                                        SemanticColorUtils.getDefaultTextColorAccent1(context)))));

        pref.setOnPreferenceClickListener(
                (unused) -> {
                    ChromeSharedPreferences.getInstance().writeBoolean(clickedPrefKey, true);
                    return false;
                });
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(isOn ? R.string.text_on : R.string.text_off);
    }

    private void showSignoutSnackbar() {
        assert getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.STARTED);
        SignOutCoordinator.showSnackbar(
                getContext(),
                ((SnackbarManager.SnackbarManageable) getActivity()).getSnackbarManager(),
                SyncServiceFactory.getForProfile(getProfile()));
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // After signing in or out of a managed account, preferences may change or become enabled
        // or disabled.
        new Handler().post(() -> updatePreferences());
    }

    @Override
    public void onSignedOut() {
        // TODO(crbug.com/343933167): The snackbar should be shown from
        // SignOutCoordinator.startSignOutFlow(), in other words SignOutCoordinator.showSnackbar()
        // should be private method.
        if (IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                == null) {
            // Show the signout snackbar, or wait until `onStart()` if the fragment is not in the
            // `STARTED` state.
            if (getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.STARTED)) {
                showSignoutSnackbar();
            } else {
                mShouldShowSnackbar = true;
            }
        }

        updatePreferences();
    }

    // TemplateUrlService.LoadListener implementation.
    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.getForProfile(getProfile()).unregisterLoadListener(this);
        updateSearchEnginePreference();
    }

    @Override
    public void syncStateChanged() {
        updateManageSyncPreference();
        updateAutofillPreferences();
    }

    public ManagedPreferenceDelegate getManagedPreferenceDelegateForTest() {
        return mManagedPreferenceDelegate;
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(getProfile())
                            .isDefaultSearchManaged();
                }
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return UserPrefs.get(getProfile())
                            .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.getForProfile(getProfile())
                            .isDefaultSearchManaged();
                }
                if (PREF_PASSWORDS.equals(preference.getKey())) {
                    return false;
                }
                return isPreferenceControlledByPolicy(preference)
                        || isPreferenceControlledByCustodian(preference);
            }
        };
    }

    public void setModalDialogManagerSupplier(
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    private boolean useLegacySettingsOrder() {
        return !ChromeFeatureList.isEnabled(
                AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID);
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
