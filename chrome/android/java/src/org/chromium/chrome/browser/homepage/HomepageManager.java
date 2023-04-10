// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/**
 * Provides information regarding homepage enabled states and URI.
 *
 * This class serves as a single homepage logic gateway.
 */
public class HomepageManager implements HomepagePolicyManager.HomepagePolicyStateListener,
                                        PartnerBrowserCustomizations.PartnerHomepageListener {
    /**
     * An interface to use for getting homepage related updates.
     */
    public interface HomepageStateListener {
        /**
         * Called when the homepage is enabled or disabled or the homepage URL changes.
         */
        void onHomepageStateUpdated();
    }

    private static HomepageManager sInstance;

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomepageStateListener> mHomepageStateListeners;
    private SettingsLauncher mSettingsLauncher;

    private HomepageManager() {
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mHomepageStateListeners = new ObserverList<>();
        HomepagePolicyManager.getInstance().addListener(this);
        PartnerBrowserCustomizations.getInstance().setPartnerHomepageListener(this);
        mSettingsLauncher = new SettingsLauncherImpl();
    }

    /**
     * Returns the singleton instance of HomepageManager, creating it if needed.
     */
    public static HomepageManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepageManager();
        }
        return sInstance;
    }

    /**
     * Adds a HomepageStateListener to receive updates when the homepage state changes.
     */
    public void addListener(HomepageStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     * @param listener The listener to remove.
     */
    public void removeListener(HomepageStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Menu click handler on home button.
     * @param context {@link Context} used for launching a settings activity.
     */
    public void onMenuClick(Context context) {
        mSettingsLauncher.launchSettingsActivity(context, HomepageSettings.class);
    }

    /**
     * Notify any listeners about a homepage state change.
     */
    public void notifyHomepageUpdated() {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onHomepageStateUpdated();
        }
    }

    /**
     * @return Whether or not homepage is enabled.
     */
    public static boolean isHomepageEnabled() {
        return HomepagePolicyManager.isHomepageManagedByPolicy()
                || getInstance().getPrefHomepageEnabled();
    }

    /**
     * @return Whether or not current homepage is customized.
     */
    public static boolean isHomepageCustomized() {
        return !HomepagePolicyManager.isHomepageManagedByPolicy()
                && !getInstance().getPrefHomepageUseDefaultUri();
    }

    /**
     * @return Whether to close the app when the user has zero tabs.
     */
    public static boolean shouldCloseAppWithZeroTabs() {
        return HomepageManager.isHomepageEnabled()
                && !UrlUtilities.isNTPUrl(HomepageManager.getHomepageUri());
    }

    /**
     * Get the current homepage URI string. If the homepage is disabled, return null; otherwise it
     * will always return a non-empty string. In cases when the homepage is specifically set as
     * empty, this function will fallback to return {@link UrlConstants.NTP_URL}.
     *
     * This function checks different source to get the current homepage, which listed below
     * according to their priority:
     *
     * <b>isManagedByPolicy > useChromeNTP > useDefaultUri > useCustomUri</b>
     *
     * @return A non-empty homepage URI string, if homepage is enabled. Null otherwise.
     *
     * @see HomepagePolicyManager#isHomepageManagedByPolicy()
     * @see #getPrefHomepageUseChromeNTP()
     * @see #getPrefHomepageUseDefaultUri()
     */
    @Nullable
    public static String getHomepageUri() {
        if (!isHomepageEnabled()) return null;

        String homepageUri = getInstance().getHomepageUriIgnoringEnabledState();
        return TextUtils.isEmpty(homepageUri) ? UrlConstants.NTP_URL : homepageUri;
    }

    /**
     * @return The default homepage URI if the homepage is partner provided or the new tab page
     *         if the homepage button is force enabled via flag.
     */
    public static String getDefaultHomepageUri() {
        if (PartnerBrowserCustomizations.getInstance().isHomepageProviderAvailableAndEnabled()) {
            return PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec();
        }

        String homepagePartnerDefaultUri;
        String homepagePartnerDefaultGurlSerialized =
                SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        if (!homepagePartnerDefaultGurlSerialized.equals("")) {
            homepagePartnerDefaultUri =
                    GURL.deserialize(homepagePartnerDefaultGurlSerialized).getSpec();
        } else {
            homepagePartnerDefaultUri = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, "");
        }
        if (!homepagePartnerDefaultUri.equals("")) return homepagePartnerDefaultUri;

        return UrlConstants.NTP_URL;
    }

    /**
     * Determines whether the homepage is set to something other than the NTP or empty/null.
     * Normally, when loading the homepage the NTP is loaded as a fallback if the homepage is null
     * or empty. So while other helper methods that check if a given string is the NTP
     * will reject null and empty, this method does the opposite.
     * @return Whether the current homepage is something other than the NTP.
     */
    public static boolean isHomepageNonNtp() {
        String currentHomepage = getHomepageUri();
        return !TextUtils.isEmpty(currentHomepage) && !UrlUtilities.isNTPUrl(currentHomepage);
    }

    /**
     * Determines whether the homepage is set to something other than the NTP or empty/null. This is
     * the same as {@link #isHomepageNonNtp()}, but uses {@link UrlUtilities#isCanonicalizedNTPUrl}
     * instead of {@link UrlUtilities#isNTPUrl} to make it possible to use before native is loaded.
     * Prefer {@link #isHomepageNonNtp()} if possible.
     * @return Whether the current homepage is something other than the NTP.
     */
    public static boolean isHomepageNonNtpPreNative() {
        String currentHomepage = getHomepageUri();
        return !TextUtils.isEmpty(currentHomepage)
                && !UrlUtilities.isCanonicalizedNTPUrl(currentHomepage);
    }

    /**
     * Get homepage URI without checking if the homepage is enabled.
     * @return Homepage URI based on policy and shared preference settings.
     */
    private @NonNull String getHomepageUriIgnoringEnabledState() {
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            return HomepagePolicyManager.getHomepageUrl().getSpec();
        }
        if (getPrefHomepageUseChromeNTP()) {
            return UrlConstants.NTP_URL;
        }
        if (getPrefHomepageUseDefaultUri()) {
            return getDefaultHomepageUri();
        }
        return getPrefHomepageCustomUri();
    }

    /**
     * Returns the user preference for whether the homepage is enabled. This doesn't take into
     * account whether the device supports having a homepage.
     *
     * @see #isHomepageEnabled
     */
    private boolean getPrefHomepageEnabled() {
        return mSharedPreferencesManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
    }

    /**
     * Sets the user preference for whether the homepage is enabled.
     */
    public void setPrefHomepageEnabled(boolean enabled) {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, enabled);
        notifyHomepageUpdated();
    }

    /**
     * @return User specified homepage custom URI string.
     */
    public String getPrefHomepageCustomUri() {
        return mSharedPreferencesManager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "");
    }

    /**
     * True if the homepage URL is the default value. False means the homepage URL is using
     * the user customized URL. Note that this method does not take enterprise policy into account.
     * Use {@link HomepagePolicyManager#isHomepageManagedByPolicy} if policy information is needed.
     *
     * @return Whether if the homepage URL is the default value.
     */
    public boolean getPrefHomepageUseDefaultUri() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
    }

    /**
     * @return Whether the homepage is set to Chrome NTP in Homepage settings
     */
    public boolean getPrefHomepageUseChromeNTP() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
    }

    /**
     * Set homepage related shared preferences, and notify listeners for the homepage status change.
     * These shared preference values will reflect what homepage we are using.
     *
     * The priority of the input pref values during value checking:
     * useChromeNTP > useDefaultUri > customUri
     *
     * @param useChromeNtp True if homepage is set as Chrome's New tab page.
     * @param useDefaultUri True if homepage is using default URI.
     * @param customUri String value for user customized homepage URI.
     *
     * @see #getHomepageUri()
     */
    public void setHomepagePreferences(
            boolean useChromeNtp, boolean useDefaultUri, String customUri) {
        boolean wasUseChromeNTP = getPrefHomepageUseChromeNTP();
        boolean wasUseDefaultUri = getPrefHomepageUseDefaultUri();
        String oldCustomUri = getPrefHomepageCustomUri();

        if (useChromeNtp == wasUseChromeNTP && useDefaultUri == wasUseDefaultUri
                && oldCustomUri.equals(customUri)) {
            return;
        }

        if (useChromeNtp != wasUseChromeNTP) {
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, useChromeNtp);
        }

        if (wasUseDefaultUri != useDefaultUri) {
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultUri);
        }

        if (!oldCustomUri.equals(customUri)) {
            mSharedPreferencesManager.writeString(
                    ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, customUri);
        }

        RecordUserAction.record("Settings.Homepage.LocationChanged_V2");
        notifyHomepageUpdated();
    }

    /**
     * Record histogram "Settings.Homepage.LocationType" with the current homepage location type.
     */
    public static void recordHomepageLocationTypeIfEnabled() {
        if (!isHomepageEnabled()) return;

        int homepageLocationType = getInstance().getHomepageLocationType();
        RecordHistogram.recordEnumeratedHistogram("Settings.Homepage.LocationType",
                homepageLocationType, HomepageLocationType.NUM_ENTRIES);
    }

    /**
     * @return {@link HomepageLocationType} for current homepage settings.
     */
    @VisibleForTesting
    public @HomepageLocationType int getHomepageLocationType() {
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            return UrlUtilities.isNTPUrl(HomepagePolicyManager.getHomepageUrl())
                    ? HomepageLocationType.POLICY_NTP
                    : HomepageLocationType.POLICY_OTHER;
        }
        if (getPrefHomepageUseChromeNTP()) {
            return HomepageLocationType.USER_CUSTOMIZED_NTP;
        }
        if (getPrefHomepageUseDefaultUri()) {
            if (!PartnerBrowserCustomizations.getInstance()
                            .isHomepageProviderAvailableAndEnabled()) {
                return HomepageLocationType.DEFAULT_NTP;
            }

            return UrlUtilities.isNTPUrl(
                           PartnerBrowserCustomizations.getInstance().getHomePageUrl())
                    ? HomepageLocationType.PARTNER_PROVIDED_NTP
                    : HomepageLocationType.PARTNER_PROVIDED_OTHER;
        }
        // If user type NTP URI as their customized homepage, we'll record user is using NTP
        return UrlUtilities.isNTPUrl(getPrefHomepageCustomUri())
                ? HomepageLocationType.USER_CUSTOMIZED_NTP
                : HomepageLocationType.USER_CUSTOMIZED_OTHER;
    }

    @Override
    public void onHomepagePolicyUpdate() {
        notifyHomepageUpdated();
    }

    @Override
    public void onHomepageUpdate() {
        notifyHomepageUpdated();
    }

    @VisibleForTesting
    public void setSettingsLauncherForTesting(SettingsLauncher launcher) {
        mSettingsLauncher = launcher;
    }
}
