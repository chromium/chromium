// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.build.NullUtil.assumeNonNull;
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
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.LifecycleEvent;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Fragment containing Safety hub. */
@NullMarked
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements SafetyHubModuleMediatorDelegate {
    private static final String PREF_UNIFIED_PASSWORDS = "passwords_unified";
    private static final String PREF_ACCOUNT_PASSWORDS = "passwords_account";
    private static final String PREF_LOCAL_PASSWORDS = "passwords_local";
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

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private SafetyHubModuleDelegate mDelegate;
    private @Nullable CallbackController mCallbackController;
    private List<SafetyHubModuleMediator> mModuleMediators;
    private @Nullable SafetyHubBrowserStateModuleMediator mBrowserStateModuleMediator;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        if (ChromeFeatureList.sSafetyHubAndroidOrganicSurvey.isEnabled()) {
            mCallbackController = new CallbackController();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::triggerOrganicHatsSurvey),
                    ORGANIC_HATS_SURVEY_DELAY_MS);
        }

        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        mPageTitle.set(getString(R.string.prefs_safety_check));
        setHasOptionsMenu(true);

        setUpModuleMediators();
        setUpSafetyTipsModule();
        updateAllModules();

        recordAllModulesState(LifecycleEvent.ON_IMPRESSION);

        // Notify the magic stack to dismiss the active module.
        if (ChromeFeatureList.sSafetyHubMagicStack.isEnabled()) {
            MagicStackBridge.getForProfile(getProfile()).dismissActiveModule();
        }
    }

    private void setUpModuleMediators() {
        SafetyHubFetchService safetyHubFetchService =
                SafetyHubFetchServiceFactory.getForProfile(getProfile());

        SafetyHubModuleMediator updateCheckModuleMediator =
                new SafetyHubUpdateCheckModuleMediator(
                        findPreference(PREF_UPDATE), this, mDelegate, safetyHubFetchService);
        SafetyHubModuleMediator permissionsRevocationModuleMediator =
                new SafetyHubPermissionsRevocationModuleMediator(
                        findPreference(PREF_UNUSED_PERMISSIONS),
                        this,
                        UnusedSitePermissionsBridge.getForProfile(getProfile()));
        SafetyHubModuleMediator safeBrowsingModuleMediator =
                new SafetyHubSafeBrowsingModuleMediator(
                        findPreference(PREF_SAFE_BROWSING), this, getProfile());

        mModuleMediators =
                new ArrayList<SafetyHubModuleMediator>(
                        Arrays.asList(
                                updateCheckModuleMediator,
                                permissionsRevocationModuleMediator,
                                safeBrowsingModuleMediator));
        boolean shouldShowNotificationModule =
                !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
                        || ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION,
                                "shadow_run",
                                true);
        if (shouldShowNotificationModule) {
            SafetyHubModuleMediator notificationsModuleMediator =
                    new SafetyHubNotificationsModuleMediator(
                            findPreference(PREF_NOTIFICATIONS_REVIEW),
                            this,
                            NotificationPermissionReviewBridge.getForProfile(getProfile()));
            mModuleMediators.add(notificationsModuleMediator);
        }

        SafetyHubAccountPasswordsDataSource accountPasswordsDataSource =
                new SafetyHubAccountPasswordsDataSource(
                        mDelegate,
                        UserPrefs.get(getProfile()),
                        safetyHubFetchService,
                        IdentityServicesProvider.get().getSigninManager(getProfile()),
                        getProfile());
        SafetyHubLocalPasswordsDataSource localPasswordsDataSource =
                new SafetyHubLocalPasswordsDataSource(
                        mDelegate,
                        UserPrefs.get(getProfile()),
                        safetyHubFetchService,
                        new PasswordStoreBridge(getProfile()));

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE)) {
            SafetyHubPasswordsModuleMediator passwordsModuleMediator =
                    new SafetyHubPasswordsModuleMediator(
                            findPreference(PREF_UNIFIED_PASSWORDS),
                            accountPasswordsDataSource,
                            localPasswordsDataSource,
                            /* mediatorDelegate= */ this,
                            mDelegate);
            mModuleMediators.add(passwordsModuleMediator);
        } else {
            SafetyHubAccountPasswordsModuleMediator accountPasswordsModuleMediator =
                    new SafetyHubAccountPasswordsModuleMediator(
                            findPreference(PREF_ACCOUNT_PASSWORDS),
                            accountPasswordsDataSource,
                            /* mediatorDelegate= */ this,
                            mDelegate);
            mModuleMediators.add(accountPasswordsModuleMediator);

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE)) {
                SafetyHubLocalPasswordsModuleMediator localPasswordsModuleMediator =
                        new SafetyHubLocalPasswordsModuleMediator(
                                findPreference(PREF_LOCAL_PASSWORDS),
                                localPasswordsDataSource,
                                /* mediatorDelegate= */ this,
                                mDelegate);
                mModuleMediators.add(localPasswordsModuleMediator);
            }
        }

        mBrowserStateModuleMediator =
                new SafetyHubBrowserStateModuleMediator(
                        findPreference(PREF_BROWSER_STATE_INDICATOR), mModuleMediators);
        mBrowserStateModuleMediator.setUpModule();

        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            moduleMediator.setUpModule();
        }
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
    public void onStart() {
        super.onStart();
        updateAllModules();

        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            // Fetch the passwords again to get the latest result.
            if (moduleMediator.getOption() == ModuleOption.ACCOUNT_PASSWORDS) {
                ((SafetyHubAccountPasswordsModuleMediator) moduleMediator)
                        .triggerNewCredentialFetch();
                break;
            }
            if (moduleMediator.getOption() == ModuleOption.LOCAL_PASSWORDS) {
                ((SafetyHubLocalPasswordsModuleMediator) moduleMediator)
                        .triggerNewCredentialFetch();
                break;
            }
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        recordAllModulesState(LifecycleEvent.ON_EXIT);

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            moduleMediator.destroy();
        }
        mModuleMediators.clear();
        if (mBrowserStateModuleMediator != null) {
            mBrowserStateModuleMediator.destroy();
            mBrowserStateModuleMediator = null;
        }
    }

    private void updateAllModules() {
        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            moduleMediator.updateModule();
        }

        onUpdateNeeded();
    }

    @Override
    public void onUpdateNeeded() {
        assumeNonNull(mBrowserStateModuleMediator);
        // `mBrowserStateModuleMediator` needs to be updated after all the other modules change, as
        // it depends on them.
        mBrowserStateModuleMediator.updateModule();

        updateAllModulesExpandState();
    }

    private void updateAllModulesExpandState() {
        boolean hasNonManagedWarningState = hasNonManagedWarningState();
        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            moduleMediator.setModuleExpandState(hasNonManagedWarningState);
        }
    }

    private boolean hasNonManagedWarningState() {
        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            if (moduleMediator.getModuleState() == ModuleState.WARNING
                    && !moduleMediator.isManaged()) {
                return true;
            }
        }
        return false;
    }

    @Initializer
    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    private void recordAllModulesState(@LifecycleEvent String event) {

        for (SafetyHubModuleMediator moduleMediator : mModuleMediators) {
            recordModuleState(
                    moduleMediator.getModuleState(),
                    getDashboardModuleTypeForModuleOption(moduleMediator.getOption()),
                    event);
        }
        assumeNonNull(mBrowserStateModuleMediator);
        @ModuleState
        int browserState =
                mBrowserStateModuleMediator.isBrowserStateSafe()
                        ? ModuleState.SAFE
                        : ModuleState.WARNING;
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

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
