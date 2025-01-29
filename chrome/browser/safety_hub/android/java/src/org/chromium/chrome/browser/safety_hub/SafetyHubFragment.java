// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleViewBinder.isBrowserStateSafe;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.getDashboardModuleTypeForModuleOption;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordModuleState;

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
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleOption;
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleState;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.LifecycleEvent;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements SafetyHubModuleMediatorDelegate {
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
    private PropertyModel mBrowserStateModule;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private CallbackController mCallbackController;

    // TODO(https://crbug.com/388788381): When this fragment no longer updates the
    // `mBrowserStateModule` directly, then use a List of the SafetyHubModuleMediators instead.
    private SafetyHubPermissionsRevocationModuleMediator mPermissionsRevocationModuleMediator;
    private SafetyHubSafeBrowsingModuleMediator mSafeBrowsingModuleMediator;
    private SafetyHubUpdateCheckModuleMediator mUpdateCheckModuleMediator;
    private SafetyHubNotificationsModuleMediator mNotificationsModuleMediator;
    private SafetyHubAccountPasswordsModuleMediator mAccountPasswordsModuleMediator;

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

        SafetyHubFetchService safetyHubFetchService =
                SafetyHubFetchServiceFactory.getForProfile(getProfile());

        mUpdateCheckModuleMediator =
                new SafetyHubUpdateCheckModuleMediator(
                        findPreference(PREF_UPDATE), this, mDelegate, safetyHubFetchService);
        mPermissionsRevocationModuleMediator =
                new SafetyHubPermissionsRevocationModuleMediator(
                        findPreference(PREF_UNUSED_PERMISSIONS),
                        this,
                        UnusedSitePermissionsBridge.getForProfile(getProfile()));
        mSafeBrowsingModuleMediator =
                new SafetyHubSafeBrowsingModuleMediator(
                        findPreference(PREF_SAFE_BROWSING), this, getProfile());
        mNotificationsModuleMediator =
                new SafetyHubNotificationsModuleMediator(
                        findPreference(PREF_NOTIFICATIONS_REVIEW),
                        this,
                        NotificationPermissionReviewBridge.getForProfile(getProfile()));
        mAccountPasswordsModuleMediator =
                new SafetyHubAccountPasswordsModuleMediator(
                        findPreference(PREF_PASSWORDS),
                        this,
                        mDelegate,
                        UserPrefs.get(getProfile()),
                        safetyHubFetchService,
                        IdentityServicesProvider.get().getSigninManager(getProfile()),
                        getProfile());

        mAccountPasswordsModuleMediator.setUpModule();
        mUpdateCheckModuleMediator.setUpModule();
        mPermissionsRevocationModuleMediator.setUpModule();
        mSafeBrowsingModuleMediator.setUpModule();
        mNotificationsModuleMediator.setUpModule();
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

    @Override
    public void launchSiteSettingsActivityForModule(@SiteSettingsCategory.Type int category) {
        launchSiteSettingsActivity(category);
    }

    private void setUpBrowserStateModule() {
        CardPreference browserStatePreference = findPreference(PREF_BROWSER_STATE_INDICATOR);
        int compromisedPasswordsCount =
                mAccountPasswordsModuleMediator.getCompromisedPasswordsCount();
        int weakPasswordsCount = mAccountPasswordsModuleMediator.getWeakPasswordsCount();
        int reusedPasswordsCount = mAccountPasswordsModuleMediator.getReusedPasswordsCount();
        int totalPasswordsCount = mAccountPasswordsModuleMediator.getTotalPasswordsCount();
        int sitesWithUnusedPermissionsCount =
                mPermissionsRevocationModuleMediator.getRevokedPermissionsCount();
        int notificationPermissionsForReviewCount =
                mNotificationsModuleMediator.getNotificationsPermissionsCount();

        mBrowserStateModule =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties.BROWSER_STATE_MODULE_KEYS)
                        .with(
                                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS,
                                mUpdateCheckModuleMediator.getUpdateStatus())
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
                                mSafeBrowsingModuleMediator.getSafeBrowsingState())
                        .build();

        PropertyModelChangeProcessor.create(
                mBrowserStateModule,
                browserStatePreference,
                DeprecatedSafetyHubModuleViewBinder::bindBrowserStateProperties);
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
        mAccountPasswordsModuleMediator.triggerNewCredentialFetch();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        recordAllModulesState(LifecycleEvent.ON_EXIT);

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mPermissionsRevocationModuleMediator != null) {
            mPermissionsRevocationModuleMediator.destroy();
            mPermissionsRevocationModuleMediator = null;
        }
        if (mSafeBrowsingModuleMediator != null) {
            mSafeBrowsingModuleMediator.destroy();
            mSafeBrowsingModuleMediator = null;
        }
        if (mNotificationsModuleMediator != null) {
            mNotificationsModuleMediator.destroy();
            mNotificationsModuleMediator = null;
        }
        if (mAccountPasswordsModuleMediator != null) {
            mAccountPasswordsModuleMediator.destroy();
            mAccountPasswordsModuleMediator = null;
        }
    }

    private void updateAllModules() {
        mUpdateCheckModuleMediator.updateModule();
        mAccountPasswordsModuleMediator.updateModule();
        mSafeBrowsingModuleMediator.updateModule();
        mPermissionsRevocationModuleMediator.updateModule();
        mNotificationsModuleMediator.updateModule();

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
            getModuleMediator(i).setModuleExpandState(hasNonManagedWarningState);
        }
    }

    private boolean hasNonManagedWarningState() {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            SafetyHubModuleMediator moduleMediator = getModuleMediator(i);
            if (moduleMediator.getModuleState() == ModuleState.WARNING
                    && !moduleMediator.isManaged()) {
                return true;
            }
        }
        return false;
    }

    private void updateBrowserStatePreference() {
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                mPermissionsRevocationModuleMediator.getRevokedPermissionsCount());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE,
                mSafeBrowsingModuleMediator.getSafeBrowsingState());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS,
                mUpdateCheckModuleMediator.getUpdateStatus());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                mNotificationsModuleMediator.getNotificationsPermissionsCount());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN,
                SafetyHubUtils.isSignedIn(getProfile()));
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                mAccountPasswordsModuleMediator.getCompromisedPasswordsCount());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT,
                mAccountPasswordsModuleMediator.getWeakPasswordsCount());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT,
                mAccountPasswordsModuleMediator.getReusedPasswordsCount());
        mBrowserStateModule.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT,
                mAccountPasswordsModuleMediator.getTotalPasswordsCount());
    }

    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    private SafetyHubModuleMediator getModuleMediator(@ModuleOption int option) {
        switch (option) {
            case ModuleOption.UNUSED_PERMISSIONS:
                return mPermissionsRevocationModuleMediator;
            case ModuleOption.SAFE_BROWSING:
                return mSafeBrowsingModuleMediator;
            case ModuleOption.UPDATE_CHECK:
                return mUpdateCheckModuleMediator;
            case ModuleOption.NOTIFICATION_REVIEW:
                return mNotificationsModuleMediator;
            case ModuleOption.ACCOUNT_PASSWORDS:
                return mAccountPasswordsModuleMediator;
            default:
                throw new IllegalArgumentException();
        }
    }

    private void recordAllModulesState(@LifecycleEvent String event) {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            recordModuleState(
                    getModuleMediator(i).getModuleState(),
                    getDashboardModuleTypeForModuleOption(i),
                    event);
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
