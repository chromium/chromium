// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleViewBinder.getModuleState;
import static org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleViewBinder.isBrowserStateSafe;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.getDashboardModuleTypeForModuleOption;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordModuleState;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordNotificationsInteraction;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleOption;
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleState;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.LifecycleEvent;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NotificationsModuleInteractions;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements NotificationPermissionReviewBridge.Observer,
                SafetyHubFetchService.Observer,
                PasswordStoreBridge.PasswordStoreObserver,
                SigninManager.SignInStateObserver,
                SafetyHubModuleMediatorDelegate {
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

    private static final int ORGANIC_HATS_SURVEY_DELAY_MS = 10000;

    private SafetyHubModuleDelegate mDelegate;
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private SafetyHubFetchService mSafetyHubFetchService;
    private PropertyModel mUpdateCheckPropertyModel;
    private PropertyModel mPasswordCheckPropertyModel;
    private PropertyModel mSafeBrowsingPropertyModel;
    private PropertyModel mNotificationsModel;
    private PropertyModel mBrowserStateModule;
    private PasswordStoreBridge mPasswordStoreBridge;
    private SigninManager mSigninManager;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private CallbackController mCallbackController;

    // TODO(https://crbug.com/388788381): When this fragment no longer updates the
    // `mBrowserStateModule` directly, then use a List of the SafetyHubModuleMediators instead.
    private SafetyHubPermissionsRevocationModuleMediator mPermissionsRevocationModuleMediator;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        if (ChromeFeatureList.sSafetyHubAndroidOrganicSurvey.isEnabled()) {
            mCallbackController = new CallbackController();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::triggerOrganicHatsSurvey),
                    ORGANIC_HATS_SURVEY_DELAY_MS);
        }

        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        mPageTitle.set(getString(R.string.prefs_safety_check));

        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mSafetyHubFetchService = SafetyHubFetchServiceFactory.getForProfile(getProfile());
        mSigninManager = IdentityServicesProvider.get().getSigninManager(getProfile());

        mPermissionsRevocationModuleMediator =
                new SafetyHubPermissionsRevocationModuleMediator(
                        findPreference(PREF_UNUSED_PERMISSIONS),
                        this,
                        UnusedSitePermissionsBridge.getForProfile(getProfile()));

        setUpAccountPasswordCheckModule();
        setUpUpdateCheckModule();
        mPermissionsRevocationModuleMediator.setUpModule();
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
            case ModuleOption.NOTIFICATION_REVIEW:
                return mNotificationsModel;
            case ModuleOption.UNUSED_PERMISSIONS:
            default:
                throw new IllegalArgumentException();
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void showSnackbarForModule(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        showSnackbar(text, identifier, controller, actionData);
    }

    @Override
    public void startSettingsForModule(Class<? extends Fragment> fragment) {
        startSettings(fragment);
    }

    private void setUpBrowserStateModule() {
        CardPreference browserStatePreference = findPreference(PREF_BROWSER_STATE_INDICATOR);
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        int weakPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        int reusedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.REUSED_CREDENTIALS_COUNT);
        int totalPasswordsCount = mDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
        int sitesWithUnusedPermissionsCount =
                mPermissionsRevocationModuleMediator.getRevokedPermissionsCount();
        int notificationPermissionsForReviewCount =
                mNotificationPermissionReviewBridge.getNotificationPermissions().size();

        mBrowserStateModule =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties.BROWSER_STATE_MODULE_KEYS)
                        .with(
                                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS,
                                mDelegate.getUpdateStatus())
                        .with(
                                DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN,
                                SafetyHubUtils.isSignedIn(getProfile()))
                        .with(
                                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT,
                                weakPasswordsCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT,
                                reusedPasswordsCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT,
                                totalPasswordsCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties
                                        .NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                                notificationPermissionsForReviewCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties
                                        .SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                                sitesWithUnusedPermissionsCount)
                        .with(
                                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE,
                                SafetyHubUtils.getSafeBrowsingState(getProfile()))
                        .build();

        PropertyModelChangeProcessor.create(
                mBrowserStateModule,
                browserStatePreference,
                DeprecatedSafetyHubModuleViewBinder::bindBrowserStateProperties);
    }

    private void setUpAccountPasswordCheckModule() {
        boolean isSignedIn = SafetyHubUtils.isSignedIn(getProfile());
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        SafetyHubExpandablePreference passwordCheckPreference = findPreference(PREF_PASSWORDS);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties
                                        .PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(DeprecatedSafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, isSignedIn)
                        .with(
                                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .build();

        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                passwordCheckPreference,
                DeprecatedSafetyHubModuleViewBinder::bindPasswordCheckProperties);
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
                                DeprecatedSafetyHubModuleProperties
                                        .UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(DeprecatedSafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                DeprecatedSafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    mDelegate.openGooglePlayStore(getContext());
                                    recordDashboardInteractions(
                                            DashboardInteractions.OPEN_PLAY_STORE);
                                })
                        .with(
                                DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    mDelegate.openGooglePlayStore(getContext());
                                    recordDashboardInteractions(
                                            DashboardInteractions.OPEN_PLAY_STORE);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mUpdateCheckPropertyModel,
                updateCheckPreference,
                DeprecatedSafetyHubModuleViewBinder::bindUpdateCheckProperties);
    }

    private void setUpNotificationsReviewModule() {
        SafetyHubExpandablePreference notificationsPreference =
                findPreference(PREF_NOTIFICATIONS_REVIEW);

        mNotificationsModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties
                                        .NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .with(DeprecatedSafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                DeprecatedSafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
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
                                DeprecatedSafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafetyHubNotificationsFragment.class);
                                    recordNotificationsInteraction(
                                            NotificationsModuleInteractions.OPEN_UI_REVIEW);
                                })
                        .with(
                                DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
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
                DeprecatedSafetyHubModuleViewBinder::bindNotificationsReviewProperties);

        mNotificationPermissionReviewBridge.addObserver(this);
    }

    private void setUpSafeBrowsingModule() {
        SafetyHubExpandablePreference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);

        mSafeBrowsingPropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_MODULE_KEYS)
                        .with(DeprecatedSafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                DeprecatedSafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafeBrowsingSettingsFragment.class);
                                    recordDashboardInteractions(
                                            DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
                                })
                        .with(
                                DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> {
                                    startSettings(SafeBrowsingSettingsFragment.class);
                                    recordDashboardInteractions(
                                            DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
                                })
                        .build();

        PropertyModelChangeProcessor.create(
                mSafeBrowsingPropertyModel,
                safeBrowsingPreference,
                DeprecatedSafetyHubModuleViewBinder::bindSafeBrowsingProperties);
    }

    private void setUpSafetyTipsModule() {
        ExpandablePreferenceGroup safetyTipsPreference = findPreference(PREF_SAFETY_TIPS);
        safetyTipsPreference.setExpanded(false);

        findPreference(PREF_SAFETY_TIPS_SAFETY_TOOLS)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            getCustomTabLauncher()
                                    .openUrlInCct(getContext(), SAFETY_TOOLS_LEARN_MORE_URL);
                            recordDashboardInteractions(
                                    DashboardInteractions.OPEN_SAFETY_TOOLS_INFO);
                            return true;
                        });

        findPreference(PREF_SAFETY_TIPS_INCOGNITO)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            getCustomTabLauncher()
                                    .openUrlInCct(getContext(), INCOGNITO_LEARN_MORE_URL);
                            recordDashboardInteractions(DashboardInteractions.OPEN_INCOGNITO_INFO);
                            return true;
                        });

        findPreference(PREF_SAFETY_TIPS_SAFE_BROWSING)
                .setOnPreferenceClickListener(
                        (preference) -> {
                            getCustomTabLauncher()
                                    .openUrlInCct(getContext(), SAFE_BROWSING_LEARN_MORE_URL);
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
            getCustomTabLauncher().openUrlInCct(getContext(), HELP_CENTER_URL);
            recordDashboardInteractions(DashboardInteractions.OPEN_HELP_CENTER);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateAllModules();

        // Fetch the passwords again to get the latest result.
        mSafetyHubFetchService.fetchCredentialsCount(success -> {});
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        recordAllModulesState(LifecycleEvent.ON_EXIT);

        mNotificationPermissionReviewBridge.removeObserver(this);
        mSafetyHubFetchService.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mPermissionsRevocationModuleMediator != null) {
            mPermissionsRevocationModuleMediator.destroy();
            mPermissionsRevocationModuleMediator = null;
        }
    }

    @Override
    public void notificationPermissionsChanged() {
        updateNotificationsReviewPreference();
    }

    @Override
    public void passwordCountsChanged() {
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
        mPermissionsRevocationModuleMediator.updateModule();
        updateNotificationsReviewPreference();

        onUpdateNeeded();
    }

    @Override
    public void onUpdateNeeded() {
        updateBrowserStatePreference();
        updateAllModulesExpandState();
    }

    private void updateAllModulesExpandState() {
        boolean hasNonManagedWarningState = hasNonManagedWarningState();

        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            // TODO(https://crbug.com/388788381): Migrate all the modules to set the module expand
            // state in their mediators.
            if (i == ModuleOption.UNUSED_PERMISSIONS) {
                mPermissionsRevocationModuleMediator.setModuleExpandState(
                        hasNonManagedWarningState);
            } else {
                updateModuleExpandState(i, hasNonManagedWarningState);
            }
        }
    }

    private boolean hasNonManagedWarningState() {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            @ModuleState int moduleState;
            boolean managed;
            // TODO(https://crbug.com/388788381): Migrate all the modules to get the module and
            // managed  state from their mediators.
            if (i == ModuleOption.UNUSED_PERMISSIONS) {
                moduleState = mPermissionsRevocationModuleMediator.getModuleState();
                managed = mPermissionsRevocationModuleMediator.isManaged();
            } else {
                PropertyModel propertyModel = getModulePropertyModel(i);
                moduleState = getModuleState(propertyModel, i);
                managed =
                        propertyModel.get(
                                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);
            }

            if (moduleState == ModuleState.WARNING && !managed) {
                return true;
            }
        }
        return false;
    }

    private void updateBrowserStatePreference() {
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                mPermissionsRevocationModuleMediator.getRevokedPermissionsCount());
    }

    private void updateModuleExpandState(
            @ModuleOption int option, boolean hasNonManagedWarningState) {
        PropertyModel propertyModel = getModulePropertyModel(option);
        @ModuleState int moduleState = getModuleState(propertyModel, option);
        boolean managed =
                propertyModel.get(DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);

        switch (moduleState) {
            case ModuleState.WARNING:
                propertyModel.set(
                        DeprecatedSafetyHubModuleProperties.IS_EXPANDED,
                        !managed || !hasNonManagedWarningState);
                break;
            case ModuleState.UNAVAILABLE:
            case ModuleState.INFO:
                propertyModel.set(
                        DeprecatedSafetyHubModuleProperties.IS_EXPANDED,
                        !hasNonManagedWarningState);
                break;
            case ModuleState.SAFE:
                propertyModel.set(DeprecatedSafetyHubModuleProperties.IS_EXPANDED, false);
                break;
            default:
                throw new IllegalArgumentException();
        }
    }

    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    private void updateNotificationsReviewPreference() {
        int notificationPermissionsForReviewCount =
                mNotificationPermissionReviewBridge.getNotificationPermissions().size();
        mNotificationsModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        onUpdateNeeded();
    }

    private void updateSafeBrowsingPreference() {
        @SafeBrowsingState int state = SafetyHubUtils.getSafeBrowsingState(getProfile());
        mSafeBrowsingPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY,
                SafetyHubUtils.isSafeBrowsingManaged(getProfile()));
        mSafeBrowsingPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, state);
        mBrowserStateModule.set(DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, state);

        onUpdateNeeded();
    }

    private void updatePasswordCheckPreference() {
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        int weakPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        int reusedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.REUSED_CREDENTIALS_COUNT);
        int totalPasswordsCount = mDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
        boolean isPasswordSavingEnabled =
                UserPrefs.get(getProfile()).getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
        boolean disabledByPolicy =
                UserPrefs.get(getProfile()).isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)
                        && !isPasswordSavingEnabled;

        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, disabledByPolicy);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN,
                SafetyHubUtils.isSignedIn(getProfile()));
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL,
                SafetyHubUtils.getAccountEmail(getProfile()));
        if (SafetyHubUtils.isSignedIn(getProfile())) {
            mPasswordCheckPropertyModel.set(
                    DeprecatedSafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                    v -> {
                        mDelegate.showPasswordCheckUi(getContext());
                        recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
                    });
            mPasswordCheckPropertyModel.set(
                    DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                    v -> {
                        mDelegate.showPasswordCheckUi(getContext());
                        recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
                    });
        } else {
            mPasswordCheckPropertyModel.set(
                    DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                    v -> {
                        mDelegate.launchSigninPromo(getContext());
                        recordDashboardInteractions(DashboardInteractions.SHOW_SIGN_IN_PROMO);
                    });
        }

        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN,
                SafetyHubUtils.isSignedIn(getProfile()));
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        onUpdateNeeded();
    }

    private void updateUpdateCheckPreference() {
        UpdateStatusProvider.UpdateStatus updateStatus = mDelegate.getUpdateStatus();
        mUpdateCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStateModule.set(DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        onUpdateNeeded();
    }

    private void recordAllModulesState(@LifecycleEvent String event) {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            @ModuleState int moduleState;
            // TODO(https://crbug.com/388788381): Migrate all the modules to get the module and
            // managed state from their mediators.
            if (i == ModuleOption.UNUSED_PERMISSIONS) {
                moduleState = mPermissionsRevocationModuleMediator.getModuleState();
            } else {
                moduleState = getModuleState(getModulePropertyModel(i), i);
            }
            recordModuleState(moduleState, getDashboardModuleTypeForModuleOption(i), event);
        }

        @ModuleState
        int browserState =
                isBrowserStateSafe(mBrowserStateModule) ? ModuleState.SAFE : ModuleState.WARNING;
        recordModuleState(browserState, DashboardModuleType.BROWSER_STATE, event);
    }

    private void triggerOrganicHatsSurvey() {
        Activity activity = getActivity();
        ViewStub hatsSurveyViewStub = activity.findViewById(R.id.hats_survey_container_stub);
        if (hatsSurveyViewStub != null && hatsSurveyViewStub.getParent() != null) {
            hatsSurveyViewStub.inflate();
        }
        SafetyHubHatsHelper safetyHubHatsHelper = SafetyHubHatsHelper.getForProfile(getProfile());
        assert safetyHubHatsHelper != null && activity != null;
        safetyHubHatsHelper.triggerOrganicHatsSurvey(activity);
    }
}
