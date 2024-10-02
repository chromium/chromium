// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsCoordinator;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillCreditCardEditor;
import org.chromium.chrome.browser.autofill.settings.AutofillLocalIbanEditor;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentBasic;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUiFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckFragmentView;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditUiFactory;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
import org.chromium.chrome.browser.privacy_sandbox.ChromeTrackingProtectionDelegate;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.TopicsManageFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragmentBase;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge;
import org.chromium.chrome.browser.safety_check.SafetyCheckCoordinator;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckUpdatesDelegateImpl;
import org.chromium.chrome.browser.safety_hub.SafetyHubBaseFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleDelegateImpl;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.privacy_sandbox.FingerprintingProtectionSettingsFragment;
import org.chromium.components.privacy_sandbox.IpProtectionSettingsFragment;
import org.chromium.components.privacy_sandbox.TrackingProtectionSettings;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Provides dependencies to fragments in the settings activity. */
public class FragmentDependencyProvider extends FragmentManager.FragmentLifecycleCallbacks {
    private final Context mContext;
    private final Profile mProfile;

    // Here are UI dependencies, i.e. objects referencing UI objects (e.g. Views). They are
    // fundamentally circular dependencies because they are needed to construct UI objects.
    // Therefore we use suppliers to provide them once they become available.
    private final OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private final OneshotSupplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    public FragmentDependencyProvider(
            Context context,
            Profile profile,
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier,
            OneshotSupplier<BottomSheetController> bottomSheetControllerSupplier,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mContext = context;
        mProfile = profile;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @Override
    public void onFragmentAttached(
            @NonNull FragmentManager fragmentManager,
            @NonNull Fragment fragment,
            @NonNull Context unusedContext) {
        // Common dependencies attachments.
        if (fragment instanceof ProfileDependentSetting) {
            ((ProfileDependentSetting) fragment).setProfile(mProfile);
        }
        if (fragment instanceof FragmentSettingsNavigation) {
            FragmentSettingsNavigation fragmentSettingsNavigation =
                    (FragmentSettingsNavigation) fragment;
            fragmentSettingsNavigation.setSettingsNavigation(
                    SettingsNavigationFactory.createSettingsNavigation());
        }

        // Settings screen specific attachments.
        if (fragment instanceof MainSettings) {
            ((MainSettings) fragment).setModalDialogManagerSupplier(mModalDialogManagerSupplier);
        }
        if (fragment instanceof BaseSiteSettingsFragment) {
            BaseSiteSettingsFragment baseSiteSettingsFragment =
                    ((BaseSiteSettingsFragment) fragment);
            ChromeSiteSettingsDelegate delegate =
                    new ChromeSiteSettingsDelegate(mContext, mProfile);
            delegate.setSnackbarManagerSupplier(mSnackbarManagerSupplier);
            baseSiteSettingsFragment.setSiteSettingsDelegate(delegate);
            baseSiteSettingsFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof SafetyCheckSettingsFragment) {
            SafetyCheckCoordinator.create(
                    (SafetyCheckSettingsFragment) fragment,
                    mProfile,
                    new SafetyCheckUpdatesDelegateImpl(),
                    new SafetyCheckBridge(mProfile),
                    SigninAndHistorySyncActivityLauncherImpl.get(),
                    SyncConsentActivityLauncherImpl.get(),
                    mModalDialogManagerSupplier,
                    SyncServiceFactory.getForProfile(mProfile),
                    UserPrefs.get(mProfile),
                    new PasswordStoreBridge(mProfile),
                    PasswordManagerHelper.getForProfile(mProfile),
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof PasswordCheckFragmentView) {
            PasswordCheckComponentUiFactory.create(
                    (PasswordCheckFragmentView) fragment,
                    LaunchIntentDispatcher::createCustomTabActivityIntent,
                    IntentUtils::addTrustedIntentExtras,
                    mProfile);
        }
        if (fragment instanceof CredentialEntryFragmentViewBase) {
            CredentialEditUiFactory.create((CredentialEntryFragmentViewBase) fragment, mProfile);
        }
        if (fragment instanceof SearchEngineSettings) {
            SearchEngineSettings settings = (SearchEngineSettings) fragment;
            settings.setDisableAutoSwitchRunnable(
                    () -> LocaleManager.getInstance().setSearchEngineAutoSwitch(false));
        }
        if (fragment instanceof ImageDescriptionsSettings) {
            ImageDescriptionsSettings imageFragment = (ImageDescriptionsSettings) fragment;
            Bundle extras = imageFragment.getArguments();
            if (extras != null) {
                extras.putBoolean(
                        ImageDescriptionsSettings.IMAGE_DESCRIPTIONS,
                        ImageDescriptionsController.getInstance()
                                .imageDescriptionsEnabled(mProfile));
                extras.putBoolean(
                        ImageDescriptionsSettings.IMAGE_DESCRIPTIONS_DATA_POLICY,
                        ImageDescriptionsController.getInstance().onlyOnWifiEnabled(mProfile));
            }
            imageFragment.setDelegate(ImageDescriptionsController.getInstance().getDelegate());
        }
        if (fragment instanceof PrivacySandboxSettingsBaseFragment) {
            PrivacySandboxSettingsBaseFragment sandboxFragment =
                    (PrivacySandboxSettingsBaseFragment) fragment;
            sandboxFragment.setSnackbarManagerSupplier(mSnackbarManagerSupplier);
            sandboxFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
            sandboxFragment.setCookieSettingsIntentHelper(
                    (Context context) -> {
                        SiteSettingsHelper.showCategorySettings(
                                context, SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
                    });
        }
        if (fragment instanceof SafeBrowsingSettingsFragmentBase) {
            SafeBrowsingSettingsFragmentBase safeBrowsingFragment =
                    (SafeBrowsingSettingsFragmentBase) fragment;
            safeBrowsingFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof LanguageSettings) {
            ((LanguageSettings) fragment)
                    .setRestartAction(
                            () -> {
                                ApplicationLifetime.terminate(true);
                            });
        }
        if (fragment instanceof ClearBrowsingDataFragmentBasic) {
            ((ClearBrowsingDataFragmentBasic) fragment)
                    .setCustomTabIntentHelper(
                            LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof PrivacyGuideFragment) {
            PrivacyGuideFragment pgFragment = (PrivacyGuideFragment) fragment;
            pgFragment.setBottomSheetControllerSupplier(mBottomSheetControllerSupplier);
            pgFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AccessibilitySettings) {
            ((AccessibilitySettings) fragment)
                    .setDelegate(new ChromeAccessibilitySettingsDelegate(mProfile));
            ((AccessibilitySettings) fragment).setPrefService(UserPrefs.get(mProfile));
        }
        if (fragment instanceof PasswordSettings) {
            ((PasswordSettings) fragment)
                    .setBottomSheetControllerSupplier(mBottomSheetControllerSupplier);
        }
        if (fragment instanceof AutofillOptionsFragment) {
            AutofillOptionsCoordinator.createFor(
                    (AutofillOptionsFragment) fragment,
                    mModalDialogManagerSupplier,
                    () -> ApplicationLifetime.terminate(true));
        }
        if (fragment instanceof TrackingProtectionSettings) {
            TrackingProtectionSettings tpFragment = ((TrackingProtectionSettings) fragment);
            tpFragment.setTrackingProtectionDelegate(
                    new ChromeTrackingProtectionDelegate(mProfile));
            tpFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AutofillCreditCardEditor) {
            ((AutofillCreditCardEditor) fragment)
                    .setModalDialogManagerSupplier(mModalDialogManagerSupplier);
        }
        if (fragment instanceof TopicsManageFragment) {
            ((TopicsManageFragment) fragment)
                    .setModalDialogManagerSupplier(mModalDialogManagerSupplier);
        }
        if (fragment instanceof IpProtectionSettingsFragment) {
            IpProtectionSettingsFragment ipProtectionSettingsFragment =
                    ((IpProtectionSettingsFragment) fragment);
            ipProtectionSettingsFragment.setTrackingProtectionDelegate(
                    new ChromeTrackingProtectionDelegate(mProfile));
            ipProtectionSettingsFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment
                instanceof FingerprintingProtectionSettingsFragment fpProtectionSettingsFragment) {
            fpProtectionSettingsFragment.setTrackingProtectionDelegate(
                    new ChromeTrackingProtectionDelegate(mProfile));
            fpProtectionSettingsFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AutofillLocalIbanEditor) {
            ((AutofillLocalIbanEditor) fragment)
                    .setModalDialogManagerSupplier(mModalDialogManagerSupplier);
        }
        if (fragment instanceof SafetyHubFragment safetyHubFragment) {
            safetyHubFragment.setDelegate(
                    new SafetyHubModuleDelegateImpl(
                            mProfile,
                            mModalDialogManagerSupplier,
                            SigninAndHistorySyncActivityLauncherImpl.get(),
                            SyncConsentActivityLauncherImpl.get()));
            // TODO(crbug.com/40751023): Create a shared interface for fragments that need access to
            // LaunchIntentDispatcher::createCustomTabActivityIntent.
            safetyHubFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AccountManagementFragment) {
            ((AccountManagementFragment) fragment)
                    .setSnackbarManagerSupplier(mSnackbarManagerSupplier);
        }
        if (fragment instanceof GoogleServicesSettings) {
            ((GoogleServicesSettings) fragment)
                    .setSnackbarManagerSupplier(mSnackbarManagerSupplier);
        }
        if (fragment instanceof ManageSyncSettings) {
            ((ManageSyncSettings) fragment).setSnackbarManagerSupplier(mSnackbarManagerSupplier);
        }
        if (fragment instanceof SafetyHubBaseFragment) {
            ((SafetyHubBaseFragment) fragment).setSnackbarManagerSupplier(mSnackbarManagerSupplier);
        }
    }
}
