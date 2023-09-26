// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.SettingsLauncherHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;
import java.util.Map;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainSettings extends ChromeBaseSettingsFragment
        implements TemplateUrlService.LoadListener, SyncService.SyncStateChangedListener,
                   SigninManager.SignInStateObserver {
    public static final String PREF_SYNC_PROMO = "sync_promo";
    public static final String PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION =
            "account_and_google_services_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_MANAGE_SYNC = "manage_sync";
    public static final String PREF_GOOGLE_SERVICES = "google_services";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";
    public static final String PREF_PRIVACY = "privacy";
    public static final String PREF_SAFETY_CHECK = "safety_check";
    public static final String PREF_NOTIFICATIONS = "notifications";
    public static final String PREF_DOWNLOADS = "downloads";
    public static final String PREF_DEVELOPER = "developer";
    public static final String PREF_AUTOFILL_OPTIONS = "autofill_options";
    public static final String PREF_AUTOFILL_ADDRESSES = "autofill_addresses";
    public static final String PREF_AUTOFILL_PAYMENTS = "autofill_payment_methods";

    private final Map<String, Preference> mAllPreferences = new HashMap<>();

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private ChromeBasePreference mManageSync;
    private @Nullable PasswordCheck mPasswordCheck;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

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
        getActivity().setTitle(R.string.settings);
        mPasswordCheck = PasswordCheckFactory.getOrCreate(new SettingsLauncherImpl());
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
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        if (getActivity().isFinishing() && mPasswordCheck != null) PasswordCheckFactory.destroy();
    }

    @Override
    public void onStart() {
        super.onStart();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/*requireUpdatedPlayServices=*/false)) {
            signinManager.addSignInStateObserver(this);
        }
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        if (signinManager.isSigninSupported(/*requireUpdatedPlayServices=*/false)) {
            signinManager.removeSignInStateObserver(this);
        }
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

        SettingsUtils.addPreferencesFromResource(this, R.xml.main_preferences);

        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(getContext());
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(getProfile());
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());

        SyncPromoPreference syncPromoPreference = findPreference(PREF_SYNC_PROMO);
        syncPromoPreference.initialize(
                profileDataCache, accountManagerFacade, signinManager, identityManager);

        SignInPreference signInPreference = findPreference(PREF_SIGN_IN);
        signInPreference.initialize(profileDataCache, accountManagerFacade,
                UserPrefs.get(getProfile()), SyncServiceFactory.getForProfile(getProfile()),
                signinManager, identityManager);

        cachePreferences();

        updateAutofillPreferences();

        // TODO(crbug.com/1373451): Remove the passwords managed subtitle for local and UPM
        // unenrolled users who can see it directly in the context of the setting.
        setManagedPreferenceDelegateForPreference(PREF_PASSWORDS);
        setManagedPreferenceDelegateForPreference(PREF_SEARCH_ENGINE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If we are on Android O+ the Notifications preference should lead to the Android
            // Settings notifications page.
            Intent intent = new Intent();
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE,
                    ContextUtils.getApplicationContext().getPackageName());
            PackageManager pm = getActivity().getPackageManager();
            if (intent.resolveActivity(pm) != null) {
                Preference notifications = findPreference(PREF_NOTIFICATIONS);
                notifications.setOnPreferenceClickListener(preference -> {
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

        new AdaptiveToolbarStatePredictor(null).recomputeUiState(uiState -> {
            // We don't show the toolbar shortcut settings page if disabled from finch.
            if (uiState.canShowUi) return;
            getPreferenceScreen().removePreference(findPreference(PREF_TOOLBAR_SHORTCUT));
        });

        if (BuildInfo.getInstance().isAutomotive) {
            getPreferenceScreen().removePreference(findPreference(PREF_SAFETY_CHECK));
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
        if (IdentityServicesProvider.get()
                        .getSigninManager(getProfile())
                        .isSigninSupported(
                                /*requireUpdatedPlayServices=*/false)) {
            addPreferenceIfAbsent(PREF_SIGN_IN);
        } else {
            removePreferenceIfPresent(PREF_SIGN_IN);
        }

        updateManageSyncPreference();
        updateSearchEnginePreference();
        updateAutofillPreferences();

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.isHomepageEnabled());

        if (NightModeUtils.isNightModeSupported()) {
            addPreferenceIfAbsent(PREF_UI_THEME)
                    .getExtras()
                    .putInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
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

    private void updateManageSyncPreference() {
        String primaryAccountName =
                CoreAccountInfo.getEmailFrom(IdentityServicesProvider.get()
                                                     .getIdentityManager(getProfile())
                                                     .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        boolean showManageSync = primaryAccountName != null;
        mManageSync.setVisible(showManageSync);
        if (!showManageSync) return;

        boolean isSyncConsentAvailable = IdentityServicesProvider.get()
                                                 .getIdentityManager(getProfile())
                                                 .getPrimaryAccountInfo(ConsentLevel.SYNC)
                != null;

        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());

        mManageSync.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity(), syncService));
        mManageSync.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity(), syncService));
        mManageSync.setOnPreferenceClickListener(pref -> {
            Context context = getContext();
            if (syncService.isSyncDisabledByEnterprisePolicy()) {
                SyncSettingsUtils.showSyncDisabledByAdministratorToast(context);
            } else if (isSyncConsentAvailable) {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(context, ManageSyncSettings.class);
            } else {
                SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                        context, SigninAccessPoint.SETTINGS_SYNC_OFF_ROW, primaryAccountName);
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
                        ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            addPreferenceIfAbsent(PREF_AUTOFILL_OPTIONS);
            Preference preference = findPreference(PREF_AUTOFILL_OPTIONS);
            preference.setFragment(null);
            preference.setOnPreferenceClickListener(unused -> {
                new SettingsLauncherImpl().launchSettingsActivity(getContext(),
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.SETTINGS));
                return true; // Means event is consumed.
            });
        } else {
            removePreferenceIfPresent(PREF_AUTOFILL_OPTIONS);
        }
        findPreference(PREF_AUTOFILL_PAYMENTS)
                .setOnPreferenceClickListener(preference
                        -> SettingsLauncherHelper.showAutofillCreditCardSettings(getActivity()));
        findPreference(PREF_AUTOFILL_ADDRESSES)
                .setOnPreferenceClickListener(preference
                        -> SettingsLauncherHelper.showAutofillProfileSettings(getActivity()));
        findPreference(PREF_PASSWORDS).setOnPreferenceClickListener(preference -> {
            PasswordManagerLauncher.showPasswordSettings(getActivity(),
                    ManagePasswordsReferrer.CHROME_SETTINGS, mModalDialogManagerSupplier,
                    /*managePasskeys=*/false);
            return true;
        });
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(isOn ? R.string.text_on : R.string.text_off);
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
}
