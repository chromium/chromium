// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.getDashboardModuleTypeForModuleOption;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordModuleState;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordNotificationsInteraction;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordRevokedPermissionsInteraction;
import static org.chromium.chrome.browser.safety_hub.SafetyHubModuleViewBinder.getModuleState;
import static org.chromium.chrome.browser.safety_hub.SafetyHubModuleViewBinder.isBrowserStateSafe;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.LifecycleEvent;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NotificationsModuleInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PermissionsModuleInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements UnusedSitePermissionsBridge.Observer,
                NotificationPermissionReviewBridge.Observer,
                SafetyHubFetchService.Observer,
                PasswordStoreBridge.PasswordStoreObserver,
                SigninManager.SignInStateObserver {
    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    private static final String PREF_PASSWORDS = "passwords_account";
    private static final String PREF_UPDATE = "update_check";
    private static final String PREF_UNUSED_PERMISSIONS = "permissions";
    private static final String PREF_NOTIFICATIONS_REVIEW = "notifications_review";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_BROWSER_STATE_INDICATOR = "browser_state_indicator";
    private static final String PREF_SAFETY_TIPS_SAFETY_TOOLS = "safety_tips_safety_tools";
    private static final String PREF_SAFETY_TIPS_INCOGNITO = "safety_tips_incognito";
    private static final String PREF_SAFETY_TIPS_SAFE_BROWSING = "safety_tips_safe_browsing";
    private static final String PREF_SAFETY_TIPS = "safety_tips";

    @VisibleForTesting
    static final String SAFETY_TOOLS_LEARN_MORE_URL = "https://www.google.com/chrome/#safe";

    @VisibleForTesting
    static final String INCOGNITO_LEARN_MORE_URL = "https://support.google.com/chrome/?p=incognito";

    @VisibleForTesting
    static final String SAFE_BROWSING_LEARN_MORE_URL =
            "https://support.google.com/chrome?p=safe_browsing_preferences";

    @VisibleForTesting
    static final String HELP_CENTER_URL = "https://support.google.com/chrome?p=safety_check";

    private SafetyHubModuleDelegate mDelegate;
    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private SafetyHubFetchService mSafetyHubFetchService;
    private PropertyModel mUpdateCheckPropertyModel;
    private PropertyModel mPasswordCheckPropertyModel;
    private PropertyModel mSafeBrowsingPropertyModel;
    private PropertyModel mPermissionsModel;
    private PropertyModel mNotificationsModel;
    private PropertyModel mBrowserStateModule;
    private CustomTabIntentHelper mCustomTabIntentHelper;
    private PasswordStoreBridge mPasswordStoreBridge;
    private SigninManager mSigninManager;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        mPageTitle.set(getString(R.string.prefs_safety_check));

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mSafetyHubFetchService = SafetyHubFetchServiceFactory.getForProfile(getProfile());
        mSigninManager = IdentityServicesProvider.get().getSigninManager(getProfile());

        setUpAccountPasswordCheckModule();
        setUpUpdateCheckModule();
        setUpPermissionsRevocationModule();
        setUpNotificationsReviewModule();
        setUpSafeBrowsingModule();
        setUpSafetyTipsModule();
        setUpBrowserStateModule();

        updateAllModules();
        recordAllModulesState(LifecycleEvent.ON_IMPRESSION);
        setHasOptionsMenu(true);

        // Notify the magic stack to dismiss the active module.
        if (ChromeFeatureList.sSafetyHubMagicStack.isEnabled()) {
            MagicStackBridge.getForProfile(getProfile()).dismissActiveModule();
        }
    }

    private PropertyModel getModulePropertyModel(@ModuleOption int option) {
        switch (option) {
            case ModuleOption.UPDATE_CHECK:
                return mUpdateCheckPropertyModel;
            case ModuleOption.ACCOUNT_PASSWORDS:
                return mPasswordCheckPropertyModel;
            case ModuleOption.SAFE_BROWSING:
                return mSafeBrowsingPropertyModel;
            case ModuleOption.UNUSED_PERMISSIONS:
                return mPermissionsModel;
            case ModuleOption.NOTIFICATION_REVIEW:
                return mNotificationsModel;
            default:
                throw new IllegalArgumentException();
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void setUpBrowserStateModule() {
        CardPreference browserStatePreference = findPreference(PREF_BROWSER_STATE_INDICATOR);
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        int totalPasswordsCount = mDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
        int sitesWithUnusedPermissionsCount =
                mUnusedSitePermissionsBridge.getRevokedPermissions().length;
        int notificationPermissionsForReviewCount =
                mNotificationPermissionReviewBridge.getNotificationPermissions().size();

        mBrowserStateModule =
                new PropertyModel.Builder(SafetyHubModuleProperties.BROWSER_STATE_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.UPDATE_STATUS, mDelegate.getUpdateStatus())
                        .with(
                                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .with(SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount)
                        .with(
                                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                                notificationPermissionsForReviewCount)
                        .with(
                                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                                sitesWithUnusedPermissionsCount)
                        .with(
                                SafetyHubModuleProperties.SAFE_BROWSING_STATE,
                                SafetyHubUtils.getSafeBrowsingState(getProfile()))
                        .build();

        PropertyModelChangeProcessor.create(
                mBrowserStateModule,
                browserStatePreference,
                SafetyHubModuleViewBinder::bindBrowserStateProperties);
    }

    private void setUpAccountPasswordCheckModule() {
        SafetyHubExpandablePreference passwordCheckPreference = findPreference(PREF_PASSWORDS);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                passwordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);
        mSafetyHubFetchService.addObserver(this);
        mSigninManager.addSignInStateObserver(this);
        if (SafetyHubUtils.isSignedIn(getProfile())) {
            mPasswordStoreBridge = new PasswordStoreBridge(getProfile());
            mPasswordStoreBridge.addObserver(this, true);
        }
    }

    private void setUpUpdateCheckModule() {
        SafetyHubExpandablePreference updateCheckPreference = findPreference(PREF_UPDATE);

        mUpdateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    mDelegate.openGooglePlayStore(getContext());
                                    recordDashboardInteractions(
                                            DashboardInteractions.OPEN_PLAY_STORE);
                                })
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    mDelegate.openGooglePlayStore(getContext());
                                    recordDashboardInteractions(
                                            DashboardInteractions.OPEN_PLAY_STORE);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mUpdateCheckPropertyModel,
                updateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);
    }

    private void setUpPermissionsRevocationModule() {
        SafetyHubExpandablePreference permissionsPreference =
                findPreference(PREF_UNUSED_PERMISSIONS);

        mPermissionsModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    PermissionsData[] permissionsDataList =
                                            mUnusedSitePermissionsBridge.getRevokedPermissions();
                                    mUnusedSitePermissionsBridge
                                            .clearRevokedPermissionsReviewList();
                                    showSnackbar(
                                            getResources()
                                                    .getQuantityString(
                                                            R.plurals
                                                                    .safety_hub_multiple_permissions_snackbar,
                                                            permissionsDataList.length,
                                                            permissionsDataList.length),
                                            Snackbar.UMA_SAFETY_HUB_REGRANT_MULTIPLE_PERMISSIONS,
                                            new SnackbarManager.SnackbarController() {
                                                @Override
                                                public void onAction(Object actionData) {
                                                    mUnusedSitePermissionsBridge
                                                            .restoreRevokedPermissionsReviewList(
                                                                    (PermissionsData[]) actionData);
                                                    recordRevokedPermissionsInteraction(
                                                            PermissionsModuleInteractions
                                                                    .UNDO_ACKNOWLEDGE_ALL);
                                                }
                                            },
                                            permissionsDataList);
                                    recordRevokedPermissionsInteraction(
                                            PermissionsModuleInteractions.ACKNOWLEDGE_ALL);
                                })
                        .with(
                                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafetyHubPermissionsFragment.class);
                                    recordRevokedPermissionsInteraction(
                                            PermissionsModuleInteractions.OPEN_REVIEW_UI);
                                })
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SiteSettings.class);
                                    recordRevokedPermissionsInteraction(
                                            PermissionsModuleInteractions.GO_TO_SETTINGS);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mPermissionsModel,
                permissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);

        mUnusedSitePermissionsBridge.addObserver(this);
    }

    private void setUpNotificationsReviewModule() {
        SafetyHubExpandablePreference notificationsPreference =
                findPreference(PREF_NOTIFICATIONS_REVIEW);

        mNotificationsModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    List<NotificationPermissions> notificationPermissionsList =
                                            mNotificationPermissionReviewBridge
                                                    .getNotificationPermissions();
                                    mNotificationPermissionReviewBridge
                                            .bulkResetNotificationPermissions();
                                    showSnackbar(
                                            getResources()
                                                    .getQuantityString(
                                                            R.plurals
                                                                    .safety_hub_notifications_bulk_reset_snackbar,
                                                            notificationPermissionsList.size(),
                                                            notificationPermissionsList.size()),
                                            Snackbar.UMA_SAFETY_HUB_MULTIPLE_SITE_NOTIFICATIONS,
                                            new SnackbarManager.SnackbarController() {
                                                @Override
                                                public void onAction(Object actionData) {
                                                    mNotificationPermissionReviewBridge
                                                            .bulkAllowNotificationPermissions(
                                                                    (List<NotificationPermissions>)
                                                                            actionData);
                                                    recordNotificationsInteraction(
                                                            NotificationsModuleInteractions
                                                                    .UNDO_BLOCK_ALL);
                                                }
                                            },
                                            notificationPermissionsList);
                                    recordNotificationsInteraction(
                                            NotificationsModuleInteractions.BLOCK_ALL);
                                })
                        .with(
                                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafetyHubNotificationsFragment.class);
                                    recordNotificationsInteraction(
                                            NotificationsModuleInteractions.OPEN_UI_REVIEW);
                                })
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    launchSiteSettingsActivity(
                                            SiteSettingsCategory.Type.NOTIFICATIONS);
                                    recordNotificationsInteraction(
                                            NotificationsModuleInteractions.GO_TO_SETTINGS);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mNotificationsModel,
                notificationsPreference,
                SafetyHubModuleViewBinder::bindNotificationsReviewProperties);

        mNotificationPermissionReviewBridge.addObserver(this);
    }

    private void setUpSafeBrowsingModule() {
        SafetyHubExpandablePreference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);

        mSafeBrowsingPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.SAFE_BROWSING_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafeBrowsingSettingsFragment.class);
                                    recordDashboardInteractions(
                                            DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
                                })
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafeBrowsingSettingsFragment.class);
                                    recordDashboardInteractions(
                                            DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mSafeBrowsingPropertyModel,
                safeBrowsingPreference,
                SafetyHubModuleViewBinder::bindSafeBrowsingProperties);
    }

    private void setUpSafetyTipsModule() {
        ExpandablePreferenceGroup safetyTipsPreference = findPreference(PREF_SAFETY_TIPS);
        safetyTipsPreference.setExpanded(false);

        findPreference(PREF_SAFETY_TIPS_SAFETY_TOOLS)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            openUrlInCct(SAFETY_TOOLS_LEARN_MORE_URL);
                            recordDashboardInteractions(
                                    DashboardInteractions.OPEN_SAFETY_TOOLS_INFO);
                            return true;
                        });

        findPreference(PREF_SAFETY_TIPS_INCOGNITO)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            openUrlInCct(INCOGNITO_LEARN_MORE_URL);
                            recordDashboardInteractions(DashboardInteractions.OPEN_INCOGNITO_INFO);
                            return true;
                        });

        findPreference(PREF_SAFETY_TIPS_SAFE_BROWSING)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            openUrlInCct(SAFE_BROWSING_LEARN_MORE_URL);
                            recordDashboardInteractions(
                                    DashboardInteractions.OPEN_SAFE_BROWSING_INFO);
                            return true;
                        });
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            openUrlInCct(HELP_CENTER_URL);
            recordDashboardInteractions(DashboardInteractions.OPEN_HELP_CENTER);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /** Sets the {@link CustomTabIntentHelper} to open urls in CCT. */
    public void setCustomTabIntentHelper(CustomTabIntentHelper helper) {
        mCustomTabIntentHelper = helper;
    }

    private void openUrlInCct(String url) {
        assert (mCustomTabIntentHelper != null)
                : "CCT helpers must be set on SafetyHubFragment before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateAllModules();

        // Fetch the passwords again to get the latest result.
        mSafetyHubFetchService.fetchBreachedCredentialsCount(success -> {});
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        recordAllModulesState(LifecycleEvent.ON_EXIT);

        mNotificationPermissionReviewBridge.removeObserver(this);
        mUnusedSitePermissionsBridge.removeObserver(this);
        mSafetyHubFetchService.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
        }
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePermissionsPreference();
    }

    @Override
    public void notificationPermissionsChanged() {
        updateNotificationsReviewPreference();
    }

    @Override
    public void compromisedPasswordCountChanged() {
        updatePasswordCheckPreference();
    }

    @Override
    public void updateStatusChanged() {
        updateUpdateCheckPreference();
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        updatePasswordCheckPreference();
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {}

    @Override
    public void onSignedIn() {
        if (mPasswordStoreBridge == null) {
            mPasswordStoreBridge = new PasswordStoreBridge(getProfile());
            mPasswordStoreBridge.addObserver(this, true);
        }
        updatePasswordCheckPreference();
    }

    @Override
    public void onSignedOut() {
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
            mPasswordStoreBridge = null;
        }
        updatePasswordCheckPreference();
    }

    private void updateAllModules() {
        updateUpdateCheckPreference();
        updatePasswordCheckPreference();
        updateSafeBrowsingPreference();
        updatePermissionsPreference();
        updateNotificationsReviewPreference();

        updateAllModulesExpandState();
    }

    private void updateAllModulesExpandState() {
        boolean hasNonManagedWarningState = hasNonManagedWarningState();

        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            updateModuleExpandState(i, hasNonManagedWarningState);
        }
    }

    private boolean hasNonManagedWarningState() {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            PropertyModel propertyModel = getModulePropertyModel(i);
            @ModuleState int moduleState = getModuleState(propertyModel, i);
            boolean managed = propertyModel.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);

            if (moduleState == ModuleState.WARNING && !managed) {
                return true;
            }
        }
        return false;
    }

    private void updateModuleExpandState(
            @ModuleOption int option, boolean hasNonManagedWarningState) {
        PropertyModel propertyModel = getModulePropertyModel(option);
        @ModuleState int moduleState = getModuleState(propertyModel, option);
        boolean managed = propertyModel.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);

        switch (moduleState) {
            case ModuleState.WARNING:
                propertyModel.set(
                        SafetyHubModuleProperties.IS_EXPANDED,
                        !managed || !hasNonManagedWarningState);
                break;
            case ModuleState.UNAVAILABLE:
            case ModuleState.INFO:
                propertyModel.set(
                        SafetyHubModuleProperties.IS_EXPANDED, !hasNonManagedWarningState);
                break;
            case ModuleState.SAFE:
                propertyModel.set(SafetyHubModuleProperties.IS_EXPANDED, false);
                break;
            default:
                throw new IllegalArgumentException();
        }
    }

    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    private void updatePermissionsPreference() {
        int sitesWithUnusedPermissionsCount =
                mUnusedSitePermissionsBridge.getRevokedPermissions().length;
        mPermissionsModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStateModule.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);

        updateAllModulesExpandState();
    }

    private void updateNotificationsReviewPreference() {
        int notificationPermissionsForReviewCount =
                mNotificationPermissionReviewBridge.getNotificationPermissions().size();
        mNotificationsModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);
        mBrowserStateModule.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        updateAllModulesExpandState();
    }

    private void updateSafeBrowsingPreference() {
        @SafeBrowsingState int state = SafetyHubUtils.getSafeBrowsingState(getProfile());
        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY,
                SafetyHubUtils.isSafeBrowsingManaged(getProfile()));
        mSafeBrowsingPropertyModel.set(SafetyHubModuleProperties.SAFE_BROWSING_STATE, state);
        mBrowserStateModule.set(SafetyHubModuleProperties.SAFE_BROWSING_STATE, state);

        updateAllModulesExpandState();
    }

    private void updatePasswordCheckPreference() {
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        int totalPasswordsCount = mDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
        boolean isPasswordSavingEnabled =
                UserPrefs.get(getProfile()).getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
        boolean disabledByPolicy =
                UserPrefs.get(getProfile()).isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)
                        && !isPasswordSavingEnabled;

        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, disabledByPolicy);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.IS_SIGNED_IN, SafetyHubUtils.isSignedIn(getProfile()));
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.ACCOUNT_EMAIL,
                SafetyHubUtils.getAccountEmail(getProfile()));
        if (SafetyHubUtils.isSignedIn(getProfile())) {
            mPasswordCheckPropertyModel.set(
                    SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                    v -> {
                        mDelegate.showPasswordCheckUI(getContext());
                        recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
                    });
            mPasswordCheckPropertyModel.set(
                    SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                    v -> {
                        mDelegate.showPasswordCheckUI(getContext());
                        recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
                    });
        } else {
            mPasswordCheckPropertyModel.set(
                    SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                    v -> {
                        mDelegate.launchSyncOrSigninPromo(getContext());
                        recordDashboardInteractions(DashboardInteractions.SHOW_SIGN_IN_PROMO);
                    });
        }

        mBrowserStateModule.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mBrowserStateModule.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        updateAllModulesExpandState();
    }

    private void updateUpdateCheckPreference() {
        UpdateStatusProvider.UpdateStatus updateStatus = mDelegate.getUpdateStatus();
        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStateModule.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        updateAllModulesExpandState();
    }

    private void recordAllModulesState(@LifecycleEvent String event) {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            @ModuleState int moduleState = getModuleState(getModulePropertyModel(i), i);
            recordModuleState(moduleState, getDashboardModuleTypeForModuleOption(i), event);
        }

        @ModuleState
        int browserState =
                isBrowserStateSafe(mBrowserStateModule) ? ModuleState.SAFE : ModuleState.WARNING;
        recordModuleState(browserState, DashboardModuleType.BROWSER_STATE, event);
    }
}
