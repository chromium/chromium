// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSnackbarController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browsing_data.content.BrowsingDataModel;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webxr.WebXrAndroidFeatureMap;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.url.GURL;

import java.util.Set;

/** A SiteSettingsDelegate instance that contains Chrome-specific Site Settings logic. */
public class ChromeSiteSettingsDelegate implements SiteSettingsDelegate {
    public static final String EMBEDDED_CONTENT_HELP_CENTER_URL =
            "https://support.google.com/chrome/?p=embedded_content";

    private final Context mContext;
    private final Profile mProfile;
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private BrowsingDataModel mBrowsingDataModel;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private PrivacySandboxSnackbarController mPrivacySandboxController;
    private LargeIconBridge mLargeIconBridge;

    public ChromeSiteSettingsDelegate(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;
        mPrivacySandboxBridge = new PrivacySandboxBridge(profile);
    }

    @Override
    public void onDestroyView() {
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }

        if (mBrowsingDataModel != null) {
            mBrowsingDataModel.destroy();
            mBrowsingDataModel = null;
        }
    }

    /** Used to set an instance of {@link SnackbarManager} by the parent activity. */
    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        snackbarManagerSupplier.onAvailable(
                (snackbarManager) -> {
                    mPrivacySandboxController =
                            new PrivacySandboxSnackbarController(mContext, snackbarManager);
                });
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mProfile;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        if (mManagedPreferenceDelegate == null) {
            mManagedPreferenceDelegate =
                    new ChromeManagedPreferenceDelegate(mProfile) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return false;
                        }
                    };
        }
        return mManagedPreferenceDelegate;
    }

    @Override
    public void getFaviconImageForURL(GURL faviconUrl, Callback<Drawable> callback) {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(mProfile);
        }
        FaviconLoader.loadFavicon(mContext, mLargeIconBridge, faviconUrl, callback);
    }

    @Override
    public boolean isBrowsingDataModelFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.BROWSING_DATA_MODEL);
    }

    @Override
    public boolean isCategoryVisible(@SiteSettingsCategory.Type int type) {
        switch (type) {
                // TODO(csharrison): Remove this condition once the experimental UI lands. It is
                // not great to dynamically remove the preference in this way.
            case SiteSettingsCategory.Type.ADS:
                return SiteSettingsCategory.adsCategoryEnabled();
            case SiteSettingsCategory.Type.ANTI_ABUSE:
                return ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVATE_STATE_TOKENS);
            case SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT:
                return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING);
            case SiteSettingsCategory.Type.BLUETOOTH:
                return ContentFeatureMap.isEnabled(
                        ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND);
            case SiteSettingsCategory.Type.BLUETOOTH_SCANNING:
                return CommandLine.getInstance()
                        .hasSwitch(ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES);
            case SiteSettingsCategory.Type.FEDERATED_IDENTITY_API:
                return ContentFeatureMap.isEnabled(ContentFeatures.FED_CM);
            case SiteSettingsCategory.Type.HAND_TRACKING:
                return PackageManagerUtils.hasSystemFeature(
                                PackageManagerUtils.XR_IMMERSIVE_FEATURE_NAME)
                        && WebXrAndroidFeatureMap.isHandTrackingEnabled();
            case SiteSettingsCategory.Type.NFC:
                return ContentFeatureMap.isEnabled(ContentFeatureList.WEB_NFC);
            case SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE:
                // Desktop Android always requests desktop sites, so hide the category.
                return !BuildConfig.IS_DESKTOP_ANDROID;
            case SiteSettingsCategory.Type.ZOOM:
                return ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)
                        && ContentFeatureMap.isEnabled(
                                ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS);
            default:
                return true;
        }
    }

    @Override
    public boolean isIncognitoModeEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled(mProfile);
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS);
    }

    @Override
    public boolean isPermissionDedicatedCpssSettingAndroidFeatureEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID);
    }

    @Override
    public boolean isPrivacySandboxFirstPartySetsUIFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI);
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return SiteChannelsManager.getInstance().getChannelIdForOrigin(origin);
    }

    @Override
    public String getAppName() {
        return mContext.getString(R.string.app_name);
    }

    @Override
    public @Nullable String getDelegateAppNameForOrigin(
            Origin origin, @ContentSettingsType.EnumType int type) {
        if (type == ContentSettingsType.NOTIFICATIONS) {
            return InstalledWebappPermissionManager.get().getDelegateAppName(origin);
        }

        return null;
    }

    @Override
    public @Nullable String getDelegatePackageNameForOrigin(
            Origin origin, @ContentSettingsType.EnumType int type) {
        if (type == ContentSettingsType.NOTIFICATIONS) {
            return InstalledWebappPermissionManager.get().getDelegatePackageName(origin);
        }

        return null;
    }

    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return true;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(
                        currentActivity,
                        currentActivity.getString(R.string.help_context_settings),
                        null);
    }

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(
                        currentActivity,
                        currentActivity.getString(R.string.help_context_protected_content),
                        null);
    }

    // TODO(crbug.com/40286347): Migrate to `HelpAndFeedbackLauncherImpl` when
    // Chrome has migrated to Open-to-Context (OTC) and new p-links work.
    @Override
    public void launchStorageAccessHelpActivity(Activity currentActivity) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(false).build();
        customTabIntent.intent.setData(Uri.parse(EMBEDDED_CONTENT_HELP_CENTER_URL));

        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        currentActivity, customTabIntent.intent);
        intent.setPackage(currentActivity.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, currentActivity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);

        IntentUtils.safeStartActivity(currentActivity, intent);
    }

    @Override
    public Set<String> getOriginsWithInstalledApp() {
        WebappRegistry registry = WebappRegistry.getInstance();
        return registry.getOriginsWithInstalledApp();
    }

    @Override
    public Set<String> getAllDelegatedNotificationOrigins() {
        return InstalledWebappPermissionManager.get().getAllDelegatedOrigins();
    }

    @Override
    public void maybeDisplayPrivacySandboxSnackbar() {
        if (mPrivacySandboxController == null) return;

        // Only show the snackbar when Privacy Sandbox APIs are enabled.
        if (!isAnyPrivacySandboxApiEnabledV4()) return;

        if (mPrivacySandboxBridge.isPrivacySandboxRestricted()) return;

        mPrivacySandboxController.showSnackbar();
    }

    private boolean isAnyPrivacySandboxApiEnabledV4() {
        PrefService prefs = UserPrefs.get(mProfile);
        return prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED)
                || prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED)
                || prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
    }

    @Override
    public void dismissPrivacySandboxSnackbar() {
        if (mPrivacySandboxController != null) {
            mPrivacySandboxController.dismissSnackbar();
        }
    }

    @Override
    public boolean isRelatedWebsiteSetsDataAccessEnabled() {
        return mPrivacySandboxBridge.isFirstPartySetsDataAccessEnabled();
    }

    @Override
    public boolean isRelatedWebsiteSetsDataAccessManaged() {
        return mPrivacySandboxBridge.isFirstPartySetsDataAccessManaged();
    }

    @Override
    public boolean isPartOfManagedRelatedWebsiteSet(String origin) {
        return mPrivacySandboxBridge.isPartOfManagedFirstPartySet(origin);
    }

    @Override
    public boolean shouldShowTrackingProtectionUI() {
        return UserPrefs.get(mProfile).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD);
    }

    @Override
    public boolean shouldShowTrackingProtectionBrandedUI() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD_UX);
    }

    @Override
    public boolean shouldShowTrackingProtectionACTFeaturesUI() {
        return shouldDisplayIpProtection() || shouldDisplayFingerprintingProtection();
    }

    @Override
    public boolean shouldDisplayIpProtection() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.IP_PROTECTION_USER_BYPASS)
                // This is copied from the `IsIpProtectionEnabled` check in the TPS API.
                && ChromeFeatureList.isEnabled(ChromeFeatureList.IP_PROTECTION_V1)
                && UserPrefs.get(mProfile).getBoolean(Pref.IP_PROTECTION_ENABLED);
    }

    @Override
    public boolean shouldDisplayFingerprintingProtection() {
        // Note: this is an interim check and will have to be updated for incognito FPP.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FINGERPRINTING_PROTECTION_USER_BYPASS);
    }

    @Override
    public boolean isBlockAll3PCDEnabledInTrackingProtection() {
        return UserPrefs.get(mProfile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED);
    }

    @Override
    public void setRelatedWebsiteSetsDataAccessEnabled(boolean enabled) {
        mPrivacySandboxBridge.setFirstPartySetsDataAccessEnabled(enabled);
    }

    @Override
    public String getRelatedWebsiteSetOwner(String memberOrigin) {
        return mPrivacySandboxBridge.getFirstPartySetOwner(memberOrigin);
    }

    @Override
    public boolean canLaunchClearBrowsingDataDialog() {
        return true;
    }

    @Override
    public void launchClearBrowsingDataDialog(Activity currentActivity) {
        if (QuickDeleteController.isQuickDeleteFollowupEnabled()) {
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(
                            currentActivity,
                            SettingsNavigation.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
        } else {
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(
                            currentActivity,
                            SettingsNavigation.SettingsFragment.CLEAR_BROWSING_DATA);
        }
    }

    @Override
    // TODO(crbug.com/40880723): Look into a more scalable pattern like
    // notifyPageOpened(String className).
    public void notifyRequestDesktopSiteSettingsPageOpened() {
        RequestDesktopUtils.notifyRequestDesktopSiteSettingsPageOpened(mProfile);
    }

    @Override
    public boolean shouldShowPrivacySandboxRwsUi() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_UI);
    }

    @Override
    public void getBrowsingDataModel(Callback<BrowsingDataModel> callback) {
        BrowsingDataBridge.buildBrowsingDataModelFromDisk(
                mProfile,
                model -> {
                    if (mBrowsingDataModel != null) {
                        mBrowsingDataModel.destroy();
                    }
                    mBrowsingDataModel = model;
                    callback.onResult(mBrowsingDataModel);
                });
    }

    @Override
    public boolean isSafetyHubEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB);
    }

    @Override
    public boolean isPermissionAutorevocationEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED);
    }

    @Override
    public void setPermissionAutorevocationEnabled(boolean isEnabled) {
        UserPrefs.get(mProfile)
                .setBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED, isEnabled);
    }
}
