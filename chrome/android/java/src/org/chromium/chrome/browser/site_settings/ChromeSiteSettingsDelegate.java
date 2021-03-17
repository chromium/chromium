// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSnackbarController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentSwitches;

import java.util.Set;

/**
 * A SiteSettingsDelegate instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeSiteSettingsDelegate implements SiteSettingsDelegate {
    // Constants for favicon processing.
    // TODO(crbug.com/1076571): Move these constants to colors.xml and dimens.xml
    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;
    // Sets the favicon corner radius to 12.5% of favicon size (2dp for a 16dp favicon)
    private static final float FAVICON_CORNER_RADIUS_FRACTION = 0.125f;
    // Sets the favicon text size to 62.5% of favicon size (10dp for a 16dp favicon)
    private static final float FAVICON_TEXT_SIZE_FRACTION = 0.625f;

    private final Context mContext;
    private final BrowserContextHandle mBrowserContext;
    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private PrivacySandboxSnackbarController mPrivacySandboxController;

    public ChromeSiteSettingsDelegate(Context context, BrowserContextHandle browserContext) {
        mContext = context;
        mBrowserContext = browserContext;
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
        return mBrowserContext;
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
    public void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback) {
        new FaviconLoader(faviconUrl, callback);
    }

    /**
     * A helper class that groups a FaviconHelper with its corresponding Callback.
     *
     * This object is kept alive by being passed to the native
     * FaviconHelper.getLocalFaviconImageForURL. Its reference will be released after the callback
     * has been called.
     */
    private class FaviconLoader implements FaviconImageCallback {
        private final String mFaviconUrl;
        private final Callback<Bitmap> mCallback;
        private final int mFaviconSizePx;
        // Loads the favicons asynchronously.
        private final FaviconHelper mFaviconHelper;

        private FaviconLoader(String faviconUrl, Callback<Bitmap> callback) {
            mFaviconUrl = faviconUrl;
            mCallback = callback;
            mFaviconSizePx =
                    mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
            mFaviconHelper = new FaviconHelper();

            // TODO(https://crbug.com/1048632): Use the current profile (i.e., regular profile or
            // incognito profile) instead of always using regular profile. It works correctly now,
            // but it is not safe.
            if (!mFaviconHelper.getLocalFaviconImageForURL(
                        Profile.getLastUsedRegularProfile(), mFaviconUrl, mFaviconSizePx, this)) {
                onFaviconAvailable(/*image=*/null, mFaviconUrl);
            }
        }

        @Override
        public void onFaviconAvailable(Bitmap image, String iconUrl) {
            mFaviconHelper.destroy();

            if (image == null) {
                // Invalid or no favicon, produce a generic one.
                Resources resources = mContext.getResources();
                float density = resources.getDisplayMetrics().density;
                int faviconSizeDp = Math.round(mFaviconSizePx / density);
                RoundedIconGenerator faviconGenerator =
                        new RoundedIconGenerator(resources, faviconSizeDp, faviconSizeDp,
                                Math.round(FAVICON_CORNER_RADIUS_FRACTION * faviconSizeDp),
                                FAVICON_BACKGROUND_COLOR,
                                Math.round(FAVICON_TEXT_SIZE_FRACTION * faviconSizeDp));
                image = faviconGenerator.generateIconForUrl(mFaviconUrl);
            }
            mCallback.onResult(image);
        }
    }

    @Override
    public boolean isCategoryVisible(@SiteSettingsCategory.Type int type) {
        switch (type) {
            // TODO(csharrison): Remove this condition once the experimental UI lands. It is not
            // great to dynamically remove the preference in this way.
            case SiteSettingsCategory.Type.ADS:
                return SiteSettingsCategory.adsCategoryEnabled();
            case SiteSettingsCategory.Type.BLUETOOTH:
                return ContentFeatureList.isEnabled(
                        ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND);
            case SiteSettingsCategory.Type.BLUETOOTH_SCANNING:
                return CommandLine.getInstance().hasSwitch(
                        ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES);
            case SiteSettingsCategory.Type.NFC:
                return ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC);
            default:
                return true;
        }
    }

    @Override
    public boolean isQuietNotificationPromptsFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS);
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
    @Nullable
    public String getDelegateAppNameForOrigin(Origin origin, @ContentSettingsType int type) {
        if (type == ContentSettingsType.NOTIFICATIONS) {
            return TrustedWebActivityPermissionManager.get().getDelegateAppName(origin);
        }

        return null;
    }

    @Override
    @Nullable
    public String getDelegatePackageNameForOrigin(Origin origin, @ContentSettingsType int type) {
        if (type == ContentSettingsType.NOTIFICATIONS) {
            return TrustedWebActivityPermissionManager.get().getDelegatePackageName(origin);
        }

        return null;
    }

    @Override
    public boolean isHelpAndFeedbackEnabled() {
        return true;
    }

    @Override
    public void launchSettingsHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_settings),
                Profile.getLastUsedRegularProfile(), null);
    }

    @Override
    public void launchProtectedContentHelpAndFeedbackActivity(Activity currentActivity) {
        HelpAndFeedbackLauncherImpl.getInstance().show(currentActivity,
                currentActivity.getString(R.string.help_context_protected_content),
                Profile.getLastUsedRegularProfile(), null);
    }

    @Override
    public Set<String> getOriginsWithInstalledApp() {
        WebappRegistry registry = WebappRegistry.getInstance();
        return registry.getOriginsWithInstalledApp();
    }

    @Override
    public Set<String> getAllDelegatedNotificationOrigins() {
        return TrustedWebActivityPermissionManager.get().getAllDelegatedOrigins();
    }

    @Override
    public void maybeDisplayPrivacySandboxSnackbar() {
        // Only show the snackbar when the Privacy Sandbox APIs are enabled.
        if (mPrivacySandboxController != null
                && PrivacySandboxBridge.isPrivacySandboxSettingsFunctional()
                && PrivacySandboxBridge.isPrivacySandboxEnabled()) {
            mPrivacySandboxController.showSnackbar();
        }
    }

    @Override
    public void dismissPrivacySandboxSnackbar() {
        if (mPrivacySandboxController != null
                && PrivacySandboxBridge.isPrivacySandboxSettingsFunctional()) {
            mPrivacySandboxController.dismissSnackbar();
        }
    }
}
