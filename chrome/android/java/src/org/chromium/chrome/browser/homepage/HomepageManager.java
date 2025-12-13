// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory.getIncognitoResolver;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory.getOriginalResolver;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomeButtonStatus;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.partnercustomizations.HomepageCharacterizationHelper;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/**
 * Provides information regarding homepage enabled states and URI.
 *
 * <p>This class serves as a single homepage logic gateway.
 */
@NullMarked
public class HomepageManager
        implements HomepagePolicyManager.HomepagePolicyStateListener,
                PartnerBrowserCustomizations.PartnerHomepageListener {
    /** An interface to use for getting homepage related updates. */
    public interface HomepageStateListener {
        /** Called when the homepage is enabled or disabled or the homepage URL changes. */
        void onHomepageStateUpdated();
    }

    private static @Nullable HomepageManager sInstance;

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomepageStateListener> mHomepageStateListeners;

    private HomepageManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mHomepageStateListeners = new ObserverList<>();
        HomepagePolicyManager.getInstance().addListener(this);
        PartnerBrowserCustomizations.getInstance().setPartnerHomepageListener(this);
    }

    /** Returns the singleton instance of HomepageManager, creating it if needed. */
    public static HomepageManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepageManager();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(HomepageManager homepageManager) {
        HomepageManager prevValue = sInstance;
        sInstance = homepageManager;
        ResettersForTesting.register(() -> sInstance = prevValue);
    }

    /** Adds a HomepageStateListener to receive updates when the homepage state changes. */
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
     *
     * @param context {@link Context} used for launching a settings activity.
     */
    public void onMenuClick(Context context) {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, HomepageSettings.class);
    }

    /** Notify any listeners about a homepage state change. */
    public void notifyHomepageUpdated() {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onHomepageStateUpdated();
        }
    }

    /**
     * @return Whether or not homepage is enabled.
     */
    public boolean isHomepageEnabled() {
        if (HomepagePolicyManager.isShowHomeButtonManaged()) {
            return HomepagePolicyManager.getShowHomeButtonValue();
        }
        return HomepagePolicyManager.isHomepageLocationManaged() || getPrefHomepageEnabled();
    }

    /**
     * @return Whether to close the app when the user has zero tabs.
     */
    public boolean shouldCloseAppWithZeroTabs() {
        // If the current homepage is the NTP, this will return true, regardless of the value of
        // isIncognito.
        return isHomepageEnabled()
                && !UrlUtilities.isNtpUrl(getHomepageGurl(/* isIncognito= */ false));
    }

    /**
     * Get the current homepage URI. If the homepage is disabled, return an empty GURL; otherwise it
     * will always return a non-empty GURL. In cases when the homepage is specifically set as empty,
     * this function will fallback to return {@link ChromeUrlConstants.nativeNtpGurl()}.
     *
     * <p>This function needs to be called on UI thread since
     * ProfileManager.getLastUsedRegularProfile() is called.
     *
     * <p>This function checks different sources to get the current homepage, which is listed below
     * according to their priority:
     *
     * <p><b>HomepageIsNtpPolicy > HomepageLocationPolicy > useChromeNtp > useDefaultGurl >
     * useCustomGurl</b>
     *
     * <p>Reference Priority Table: crbug.com/400800634#comment7
     *
     * @return A non-empty GURL, if homepage is enabled. An empty GURL otherwise.
     * @see HomepagePolicyManager#isHomepageLocationManaged()
     * @see #getPrefHomepageUseChromeNtp()
     * @see #getPrefHomepageUseDefaultUri()
     */
    public GURL getHomepageGurl(boolean isIncognito) {
        if (HomepagePolicyManager.isHomepageNewTabPageEnabled()) {
            return getNtpUrl(isIncognito);
        }

        if (!isHomepageEnabled()) return GURL.emptyGURL();

        GURL homepageGurl = getHomepageGurlIgnoringEnabledState(isIncognito);
        if (homepageGurl.isEmpty()) {
            homepageGurl = getNtpUrl(isIncognito);
        }

        return homepageGurl;
    }

    /**
     * @return A GURL for the default homepage URI if the homepage is partner provided, or the new
     *     tab page if the homepage button is force enabled via flag.
     */
    public GURL getDefaultHomepageGurl(boolean isIncognito) {
        if (PartnerBrowserCustomizations.getInstance().isHomepageProviderAvailableAndEnabled()) {
            return assumeNonNull(PartnerBrowserCustomizations.getInstance().getHomePageUrl());
        }

        String homepagePartnerDefaultGurlSerialized =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        if (!homepagePartnerDefaultGurlSerialized.equals("")) {
            GURL homepagePartnerDefaultGurl =
                    GURL.deserialize(homepagePartnerDefaultGurlSerialized);
            if (!homepagePartnerDefaultGurl.isEmpty()) {
                return homepagePartnerDefaultGurl;
            }
        }

        String homepagePartnerDefaultUri =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys
                                        .DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                                "");
        if (!homepagePartnerDefaultUri.equals("")) {
            GURL homepagePartnerDefaultGurl = new GURL(homepagePartnerDefaultUri);
            if (homepagePartnerDefaultGurl.isValid()) {
                ChromeSharedPreferences.getInstance()
                        .writeString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL,
                                homepagePartnerDefaultGurl.serialize());
                ChromeSharedPreferences.getInstance()
                        .removeKey(
                                ChromePreferenceKeys
                                        .DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI);
                return homepagePartnerDefaultGurl;
            }
        }

        return getNtpUrl(isIncognito);
    }

    /**
     * Determines whether the homepage is set to something other than the NTP or empty/null.
     * Normally, when loading the homepage the NTP is loaded as a fallback if the homepage is null
     * or empty. So while other helper methods that check if a given string is the NTP will reject
     * null and empty, this method does the opposite.
     *
     * @return Whether the current homepage is something other than the NTP.
     */
    public boolean isHomepageNonNtp() {
        // If the current homepage is the NTP, this will return true, regardless of the value of
        // isIncognito.
        @Nullable GURL currentHomepage = getHomepageGurl(/* isIncognito= */ false);
        return currentHomepage != null
                && !currentHomepage.isEmpty()
                && !UrlUtilities.isNtpUrl(currentHomepage);
    }

    /**
     * Get homepage URI without checking if the homepage is enabled.
     *
     * @return Homepage GURL based on policy and shared preference settings.
     */
    private GURL getHomepageGurlIgnoringEnabledState(boolean isIncognito) {
        if (HomepagePolicyManager.isHomepageNewTabPageEnabled()) {
            return getNtpUrl(isIncognito);
        }
        if (HomepagePolicyManager.isHomepageLocationManaged()) {
            return HomepagePolicyManager.getHomepageUrl();
        }
        if (getPrefHomepageUseChromeNtp()) {
            return getNtpUrl(isIncognito);
        }
        if (getPrefHomepageUseDefaultUri()) {
            return getDefaultHomepageGurl(isIncognito);
        }
        return getPrefHomepageCustomGurl();
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

    /** Sets the user preference for whether the homepage is enabled. */
    public void setPrefHomepageEnabled(boolean enabled) {
        HomepagePolicyManager.setNativeShowHomeButtonState(enabled);
        setJavaPrefHomepageEnabled(enabled);
    }

    /**
     * Sets only the Java user preference for whether the homepage is enabled. Used for testing when
     * native is not initialized.
     */
    @VisibleForTesting
    public void setJavaPrefHomepageEnabled(boolean enabled) {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, enabled);
        notifyHomepageUpdated();
    }

    /**
     * @return User specified homepage custom GURL.
     */
    public GURL getPrefHomepageCustomGurl() {
        String homepageCustomGurlSerialized =
                mSharedPreferencesManager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, "");
        if (!homepageCustomGurlSerialized.equals("")) {
            return GURL.deserialize(homepageCustomGurlSerialized);
        }

        String homepageCustomUri =
                mSharedPreferencesManager.readString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, "");
        if (!homepageCustomUri.equals("")) {
            GURL homepageCustomGurl = new GURL(homepageCustomUri);
            if (homepageCustomGurl.isValid()) {
                mSharedPreferencesManager.writeString(
                        ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, homepageCustomGurl.serialize());
                mSharedPreferencesManager.removeKey(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI);
                return homepageCustomGurl;
            }
        }

        return GURL.emptyGURL();
    }

    /**
     * True if the homepage URL is the default value. False means the homepage URL is using the user
     * customized URL. Note that this method does not take enterprise policy into account. Use
     * {@link HomepagePolicyManager#isHomepageLocationManaged} if policy information is needed.
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
    public boolean getPrefHomepageUseChromeNtp() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
    }

    /**
     * Set homepage related shared preferences, and notify listeners for the homepage status change.
     * These shared preference values will reflect what homepage we are using.
     *
     * <p>The priority of the input pref values during value checking: useChromeNtp > useDefaultGurl
     * > customGurl
     *
     * @param useChromeNtp True if homepage is set as Chrome's New tab page.
     * @param useDefaultGurl True if homepage is using default URI.
     * @param customGurl A GURL for the user customized homepage URI.
     * @see #getHomepageGurl()
     */
    public void setHomepageSelection(
            boolean useChromeNtp, boolean useDefaultGurl, GURL customGurl) {
        // Update the native state.
        HomepagePolicyManager.setNativeHomepageIsNtp(useChromeNtp);
        if (!useChromeNtp) {
            HomepagePolicyManager.setNativeHomepageLocation(customGurl.serialize());
        }
        // Update Java state.
        setJavaHomepageSelection(useChromeNtp, useDefaultGurl, customGurl);
    }

    public void setJavaHomepageSelection(
            boolean useChromeNtp, boolean useDefaultGurl, GURL customGurl) {
        boolean wasUseChromeNtp = getPrefHomepageUseChromeNtp();
        boolean wasUseDefaultUri = getPrefHomepageUseDefaultUri();
        GURL oldCustomGurl = getPrefHomepageCustomGurl();

        if (useChromeNtp == wasUseChromeNtp
                && useDefaultGurl == wasUseDefaultUri
                && oldCustomGurl.equals(customGurl)) {
            return;
        }

        if (useChromeNtp != wasUseChromeNtp) {
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, useChromeNtp);
        }

        if (wasUseDefaultUri != useDefaultGurl) {
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, useDefaultGurl);
        }

        if (!oldCustomGurl.equals(customGurl)) {
            mSharedPreferencesManager.writeString(
                    ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, customGurl.serialize());
        }

        RecordUserAction.record("Settings.Homepage.LocationChanged_V2");
        notifyHomepageUpdated();
    }

    /**
     * Record histogram "Settings.Homepage.LocationType" with the current homepage location type.
     */
    public void recordHomepageLocationTypeIfEnabled() {
        if (!isHomepageEnabled()) return;

        int homepageLocationType = getHomepageLocationType();
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.Homepage.LocationType",
                homepageLocationType,
                HomepageLocationType.NUM_ENTRIES);
    }

    /**
     * @return {@link HomepageLocationType} for current homepage settings.
     */
    @VisibleForTesting
    public @HomepageLocationType int getHomepageLocationType() {
        if (HomepagePolicyManager.isHomepageNewTabPageEnabled()) {
            return HomepageLocationType.POLICY_NTP;
        }
        if (HomepagePolicyManager.isHomepageLocationManaged()) {
            return UrlUtilities.isNtpUrl(HomepagePolicyManager.getHomepageUrl())
                    ? HomepageLocationType.POLICY_NTP
                    : HomepageLocationType.POLICY_OTHER;
        }
        if (getPrefHomepageUseChromeNtp()) {
            return HomepageLocationType.USER_CUSTOMIZED_NTP;
        }
        if (getPrefHomepageUseDefaultUri()) {
            if (!PartnerBrowserCustomizations.getInstance()
                    .isHomepageProviderAvailableAndEnabled()) {
                return HomepageLocationType.DEFAULT_NTP;
            }

            GURL partnerHomePageUrl = PartnerBrowserCustomizations.getInstance().getHomePageUrl();
            return partnerHomePageUrl != null && UrlUtilities.isNtpUrl(partnerHomePageUrl)
                    ? HomepageLocationType.PARTNER_PROVIDED_NTP
                    : HomepageLocationType.PARTNER_PROVIDED_OTHER;
        }
        // If user type NTP URI as their customized homepage, we'll record user is using NTP.
        return UrlUtilities.isNtpUrl(getPrefHomepageCustomGurl())
                ? HomepageLocationType.USER_CUSTOMIZED_NTP
                : HomepageLocationType.USER_CUSTOMIZED_OTHER;
    }

    /**
     * Record histogram "Settings.Homepage.HomeButtonStatus" with the current homepage location
     * type.
     */
    public void recordHomepageButtonStatus() {
        int homeButtonStatus = getHomeButtonStatus();
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.Homepage.HomeButtonStatus",
                homeButtonStatus,
                HomeButtonStatus.NUM_ENTRIES);
    }

    /**
     * @return {@link HomeButtonStatus} for current homepage settings.
     */
    @VisibleForTesting
    public @HomeButtonStatus int getHomeButtonStatus() {
        if (HomepagePolicyManager.isShowHomeButtonManaged()) {
            return HomepagePolicyManager.getShowHomeButtonValue()
                    ? HomeButtonStatus.POLICY_ON
                    : HomeButtonStatus.POLICY_OFF;
        }
        if (HomepagePolicyManager.isHomepageLocationManaged()
                || HomepagePolicyManager.isHomepageNewTabPageManaged()) {
            return HomeButtonStatus.POLICY_ON;
        }
        return getPrefHomepageEnabled() ? HomeButtonStatus.USER_ON : HomeButtonStatus.USER_OFF;
    }

    @Override
    public void onHomepagePolicyUpdate() {
        notifyHomepageUpdated();
    }

    @Override
    public void onHomepageUpdate() {
        notifyHomepageUpdated();
    }

    /**
     * Provides a helper for UMA reporting of partner customization and Homepage outcomes.
     *
     * @return A {@link HomepageCharacterizationHelper} that provides access to a few helpful
     *     methods.
     */
    public static HomepageCharacterizationHelper getHomepageCharacterizationHelper() {
        return new HomepageCharacterizationHelper() {
            @Override
            public boolean isUrlNtp(@Nullable String url) {
                return url != null
                        && (UrlConstants.NTP_URL.equals(url) || UrlUtilities.isNtpUrl(url));
            }

            @Override
            public boolean isPartner() {
                switch (getInstance().getHomepageLocationType()) {
                    case HomepageLocationType.PARTNER_PROVIDED_OTHER:
                    case HomepageLocationType.PARTNER_PROVIDED_NTP:
                        return true;
                    default:
                        return false;
                }
            }

            @Override
            public boolean isNtp() {
                switch (getInstance().getHomepageLocationType()) {
                    case HomepageLocationType.POLICY_NTP:
                    case HomepageLocationType.PARTNER_PROVIDED_NTP:
                    case HomepageLocationType.USER_CUSTOMIZED_NTP:
                    case HomepageLocationType.DEFAULT_NTP:
                        return true;
                    default:
                        return false;
                }
            }
        };
    }

    /**
     * Returns a GURL representing the NTP. This GURL can be used prior to native initialization.
     */
    private static GURL getNtpUrl(boolean isIncognito) {
        UrlConstantResolver resolver = isIncognito ? getIncognitoResolver() : getOriginalResolver();
        return resolver.getNtpGurl();
    }
}
