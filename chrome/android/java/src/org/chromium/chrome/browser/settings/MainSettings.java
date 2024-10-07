// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.SettingsNavigationHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordAccessLossDialogHelper;
import org.chromium.chrome.browser.password_manager.PasswordExportLauncher;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.password_manager.settings.PasswordsPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.settings_promo_card.SettingsPromoCardPreference;
import org.chromium.chrome.browser.ui.signin.SignOutCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;
import java.util.Map;

/** The main settings screen, shown when the user first opens Settings. */
public class MainSettings extends ChromeBaseSettingsFragment
        implements TemplateUrlService.LoadListener,
                SyncService.SyncStateChangedListener,
                SigninManager.SignInStateObserver {
    public static final String PREF_SYNC_PROMO = "sync_promo";
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

    public MainSettings() {
        setHasOptionsMenu(true);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
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
    public void onViewCreated(View view, Bundle savedInstanceState) {
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
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    private void createPreferences() {
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        SettingsUtils.addPreferencesFromResource(
                this,
                useLegacySettingsOrder() ? R.xml.main_preferences_legacy : R.xml.main_preferences);

        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(getContext());
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());

        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();

        if (SigninFeatureMap.isEnabled(SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)) {
            // TODO(crbug.com/364906215): Define SettingsPromoCardPreference in the xml once
            // SyncPromoPreference is removed.
            SettingsPromoCardPreference settingsPromoCardPreference =
                    new SettingsPromoCardPreference(
                            getContext(), null, TrackerFactory.getTrackerForProfile(getProfile()));
            settingsPromoCardPreference.setKey(PREF_SETTINGS_PROMO_CARD);
            settingsPromoCardPreference.setOrder(0);
            getPreferenceScreen().addPreference(settingsPromoCardPreference);
        } else {
            SyncPromoPreference syncPromoPreference = new SyncPromoPreference(getContext(), null);
            syncPromoPreference.setKey(PREF_SYNC_PROMO);
            syncPromoPreference.setOrder(0);
            syncPromoPreference.initialize(
                    profileDataCache,
                    accountManagerFacade,
                    signinManager,
                    identityManager,
                    new SyncPromoController(
                            getProfile(),
                            bottomSheetStrings,
                            SigninAccessPoint.SETTINGS,
                            SyncConsentActivityLauncherImpl.get(),
                            SigninAndHistorySyncActivityLauncherImpl.get()));
            getPreferenceScreen().addPreference(syncPromoPreference);
        }

        SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
        signInPreference.initialize(getProfile(), profileDataCache, accountManagerFacade);

        updateGoogleServicePreference();
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

        new AdaptiveToolbarStatePredictor(getContext(), getProfile(), null)
                .recomputeUiState(
                        uiState -> {
                            // We don't show the toolbar shortcut settings page if disabled from
                            // finch.
                            if (uiState.canShowUi) return;
                            getPreferenceScreen()
                                    .removePreference(findPreference(PREF_TOOLBAR_SHORTCUT));
                        });

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
        if (SigninFeatureMap.isEnabled(SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)) {
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

        boolean isTabGroupSyncAutoOpenConfigurable =
                TabGroupSyncFeatures.isTabGroupSyncEnabled(getProfile())
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH);
        if (isTabGroupSyncAutoOpenConfigurable
                || TabUiFeatureUtilities.isTabGroupCreationDialogShowConfigurable()
                || ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_TAB_DECLUTTER)) {
            addPreferenceIfAbsent(PREF_TABS);
        } else {
            removePreferenceIfPresent(PREF_TABS);
        }

        if (ToolbarPositionController.isToolbarPositionCustomizationEnabled(getContext(), false)) {
            addPreferenceIfAbsent(PREF_ADDRESS_BAR);
        } else {
            removePreferenceIfPresent(PREF_ADDRESS_BAR);
        }

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.getInstance().isHomepageEnabled());

        if (HomeModulesConfigManager.getInstance().hasModuleShownInSettings()) {
            addPreferenceIfAbsent(PREF_HOME_MODULES_CONFIG);
        } else {
            removePreferenceIfPresent(PREF_HOME_MODULES_CONFIG);
        }

        if (NightModeUtils.isNightModeSupported()) {
            Preference themePref = addPreferenceIfAbsent(PREF_UI_THEME);
            themePref
                    .getExtras()
                    .putInt(
                            ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                            ThemeSettingsEntry.SETTINGS);
        } else {
            removePreferenceIfPresent(PREF_UI_THEME);
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

    private void updateGoogleServicePreference() {
        ChromeBasePreference googleServicePreference = findPreference(PREF_GOOGLE_SERVICES);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            googleServicePreference.setIcon(R.drawable.ic_google_services_48dp_with_bg);
            googleServicePreference.setViewId(R.id.account_management_google_services_row);
        } else {
            Drawable googleServicesIcon =
                    UiUtils.getTintedDrawable(
                            getContext(),
                            R.drawable.ic_google_services_48dp,
                            R.color.default_icon_color_tint_list);
            googleServicePreference.setIcon(googleServicesIcon);
        }
    }

    private void updateManageSyncPreference() {
        String primaryAccountName =
                CoreAccountInfo.getEmailFrom(
                        IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SIGNIN));

        // TODO(crbug.com/40067770): Remove usage of ConsentLevel.SYNC after kSync users are
        // migrated to kSignin in phase 3. See ConsentLevel::kSync documentation for details.
        boolean isSyncConsentAvailable =
                IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null;
        boolean showManageSync =
                primaryAccountName != null
                        && (!ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                                || isSyncConsentAvailable);
        mManageSync.setVisible(showManageSync);
        if (!showManageSync) return;

        mManageSync.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity(), getProfile()));
        mManageSync.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity(), getProfile()));

        mManageSync.setOnPreferenceClickListener(
                pref -> {
                    Context context = getContext();
                    if (SyncServiceFactory.getForProfile(getProfile())
                            .isSyncDisabledByEnterprisePolicy()) {
                        SyncSettingsUtils.showSyncDisabledByAdministratorToast(context);
                    } else if (isSyncConsentAvailable) {
                        SettingsNavigation settingsNavigation =
                                SettingsNavigationFactory.createSettingsNavigation();
                        settingsNavigation.startSettings(context, ManageSyncSettings.class);
                    } else {
                        // TODO(crbug.com/40067770): Remove after rolling out
                        // REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS.
                        assert !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
                        SyncConsentActivityLauncherImpl.get()
                                .launchActivityForPromoDefaultFlow(
                                        context,
                                        SigninAccessPoint.SETTINGS_SYNC_OFF_ROW,
                                        primaryAccountName);
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

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList
                        .UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)) {
            // This is temporary code needed for migrating people to UPM. With UPM there is no
            // longer passwords setting page in Chrome, so we need to ask users to export their
            // passwords here, in main settings.
            boolean startPasswordsExportFlow =
                    getArguments() != null
                            && getArguments()
                                    .containsKey(PasswordExportLauncher.START_PASSWORDS_EXPORT)
                            && getArguments()
                                    .getBoolean(PasswordExportLauncher.START_PASSWORDS_EXPORT);
            if (startPasswordsExportFlow) {
                PasswordAccessLossDialogHelper.launchExportFlow(
                        getContext(), getProfile(), mModalDialogManagerSupplier);
                getArguments().putBoolean(PasswordExportLauncher.START_PASSWORDS_EXPORT, false);
            }
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

        // onSignedOut() is also called when a supervised account revokes the sync consent without
        // signing out, in this case the Snackbar should not be shown.
        if (IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                        == null
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
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
}
