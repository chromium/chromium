// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSnackbarController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.url.GURL;

import java.util.Set;

/**
 * A SiteSettingsDelegate instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeSiteSettingsDelegate implements SiteSettingsDelegate {
    private final Context mContext;
    private final Profile mProfile;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private PrivacySandboxSnackbarController mPrivacySandboxController;
    private LargeIconBridge mLargeIconBridge;

    public ChromeSiteSettingsDelegate(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;
    }

    @Override
    public void onDestroyView() {
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }
    }

    /**
     * Used to set an instance of {@link SnackbarManager} by the parent activity.
     */
    public void setSnackbarManager(SnackbarManager manager) {
        if (manager != null) {
            mPrivacySandboxController = new PrivacySandboxSnackbarController(
                    mContext, manager, new SettingsLauncherImpl());
        }
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mProfile;
    }

    @Override
    public ManagedPreferenceDelegate getManagedPreferenceDelegate() {
        if (mManagedPreferenceDelegate == null) {
            mManagedPreferenceDelegate = new ChromeManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }
            };
        }
        return mManagedPreferenceDelegate;
    }

    @Override
    public void resetZoomLevel(String host) {
        // TODO(crbug.com/1459631): Add delete logic here.
    }

    @Override
    public void getFaviconImageForURL(GURL faviconUrl, Callback<Drawable> callback) {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(mProfile);
        }
        FaviconLoader.loadFavicon(mContext, mLargeIconBridge, faviconUrl, callback);
    }

    @Override
    public boolean isCategoryVisible(@SiteSettingsCategory.Type int type) {
        switch (type) {
            // TODO(csharrison): Remove this condition once the experimental UI lands. It is not
            // great to dynamically remove the preference in this way.
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
                return CommandLine.getInstance().hasSwitch(
                        ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES);
            case SiteSettingsCategory.Type.FEDERATED_IDENTITY_API:
                return ContentFeatureMap.isEnabled(ContentFeatures.FED_CM);
            case SiteSettingsCategory.Type.NFC:
                return ContentFeatureMap.isEnabled(ContentFeatureList.WEB_NFC);
            case SiteSettingsCategory.Type.ZOOM:
                return ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM);
            default:
                return true;
        }
    }

    @Override
    public boolean isIncognitoModeEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled();
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS);
    }

    @Override
    public boolean isPrivacySandboxFirstPartySetsUIFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI);
    }

    @Override
    public boolean isPrivacySandboxSettings4Enabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4);
    }

    @Override
    public boolean isUserBypassUIEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.USER_BYPASS_UI);
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
            Origin origin, @ContentSettingsType int type) {
        if (type == ContentSettingsType.NOTIFICATIONS) {
            return InstalledWebappPermissionManager.get().getDelegateAppName(origin);
        }

        return null;
    }

    @Override
    public @Nullable String getDelegatePackageNameForOrigin(
            Origin origin, @ContentSettingsType int type) {
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
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile).show(
                currentActivity, currentActivity.getString(R.string.help_context_settings), null);
    }

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile).show(currentActivity,
                currentActivity.getString(R.string.help_context_protected_content), null);
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
        if (isPrivacySandboxSettings4Enabled()) {
            if (!isAnyPrivacySandboxApiEnabledV4()) return;
        } else {
            if (!PrivacySandboxBridge.isPrivacySandboxEnabled()) return;
        }

        if (PrivacySandboxBridge.isPrivacySandboxRestricted()) return;

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
    public boolean isFirstPartySetsDataAccessEnabled() {
        return PrivacySandboxBridge.isFirstPartySetsDataAccessEnabled();
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged() {
        return PrivacySandboxBridge.isFirstPartySetsDataAccessManaged();
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(String origin) {
        return PrivacySandboxBridge.isPartOfManagedFirstPartySet(origin);
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(boolean enabled) {
        PrivacySandboxBridge.setFirstPartySetsDataAccessEnabled(enabled);
    }

    @Override
    public String getFirstPartySetOwner(String memberOrigin) {
        return PrivacySandboxBridge.getFirstPartySetOwner(memberOrigin);
    }

    @Override
    public boolean canLaunchClearBrowsingDataDialog() {
        return true;
    }

    @Override
    public void launchClearBrowsingDataDialog(Activity currentActivity) {
        new SettingsLauncherImpl().launchSettingsActivity(
                currentActivity, ClearBrowsingDataTabsFragment.class);
    }

    @Override
    // TODO(crbug.com/1393116): Look into a more scalable pattern like
    // notifyPageOpened(String className).
    public void notifyRequestDesktopSiteSettingsPageOpened() {
        RequestDesktopUtils.notifyRequestDesktopSiteSettingsPageOpened();
    }
}
