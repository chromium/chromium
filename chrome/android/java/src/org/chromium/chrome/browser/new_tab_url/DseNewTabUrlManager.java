// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.new_tab_url;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.GURL;

/**
 * A central class for feature NewTabSearchEngineUrlAndroid which swaps out NTP if the default
 * search engine isn't Google. It holds a reference of {@link TemplateUrlService} and observes the
 * DSE changes to update the cached values in the SharedPreference.
 */
public class DseNewTabUrlManager {
    private ObservableSupplier<Profile> mProfileSupplier;
    private Callback<Profile> mProfileCallback;
    private TemplateUrlService mTemplateUrlService;
    private TemplateUrlServiceObserver mTemplateUrlServiceObserver;

    private static final String SWAP_OUT_NTP_PARAM = "swap_out_ntp";
    public static final BooleanCachedFieldTrialParameter SWAP_OUT_NTP =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.NEW_TAB_SEARCH_ENGINE_URL_ANDROID, SWAP_OUT_NTP_PARAM, false);

    public DseNewTabUrlManager(ObservableSupplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;
        mProfileCallback = this::onProfileAvailable;
        mProfileSupplier.addObserver(mProfileCallback);
    }

    /**
     * Returns the new Tab URL of the default search engine if should override any NTP's URL.
     * Returns the given URL if don't need to override.
     * @param gurl The GURL to check.
     */
    public GURL maybeGetOverrideUrl(GURL gurl) {
        if (isIncognito()
                || !shouldSwapOutNtp()
                || isDefaultSearchEngineGoogle()
                || !UrlUtilities.isNtpUrl(gurl)) {
            return gurl;
        }

        String newTabUrl = getDSENewTabUrl(mTemplateUrlService);
        return newTabUrl != null ? new GURL(newTabUrl) : gurl;
    }

    public void destroy() {
        if (mProfileSupplier != null && mProfileCallback != null) {
            mProfileSupplier.removeObserver(mProfileCallback);
            mProfileCallback = null;
            mProfileSupplier = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(mTemplateUrlServiceObserver);
            mTemplateUrlServiceObserver = null;
            mTemplateUrlService = null;
        }
    }

    /**
     * Returns the new Tab URL of the default search engine if should override any NTP's URL.
     * Returns the given URL if don't need to override.
     *
     * @param gurl The URL to check.
     * @param profile The instance of the current {@link Profile}.
     */
    public static GURL maybeGetOverrideUrl(GURL gurl, Profile profile) {
        if ((profile != null && profile.isOffTheRecord())
                || !shouldSwapOutNtp()
                || isDefaultSearchEngineGoogle()
                || !UrlUtilities.isNtpUrl(gurl)) {
            return gurl;
        }

        TemplateUrlService templateUrlService =
                profile != null ? TemplateUrlServiceFactory.getForProfile(profile) : null;
        String newTabUrl = getDSENewTabUrl(templateUrlService);
        return newTabUrl != null ? new GURL(newTabUrl) : gurl;
    }

    /** Returns whether the feature NewTabSearchEngineUrlAndroid is enabled. */
    public static boolean isNewTabSearchEngineUrlAndroidEnabled() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY, false);
    }

    /**
     * Returns whether the parameter SWAP_OUT_NTP is enabled. Note: this method only checks parts of
     * isNewTabSearchEngineUrlAndroidEnabled(), i.e., it doesn't check country code.
     */
    public static boolean isSwapOutNtpFlagEnabled() {
        return SWAP_OUT_NTP.getValue();
    }

    /**
     * Returns cached value of {@link ChromePreferenceKeys.IS_DSE_GOOGLE} in the SharedPreference.
     */
    public static boolean isDefaultSearchEngineGoogle() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, true);
    }

    /**
     * Returns the new Tab URL of the default search engine:
     * 1. Returns the cached value ChromePreferenceKeys.DSE_NEW_TAB_URL in the SharedPreference if
     *    the templateUrlService is null.
     * 2. Returns null if the DSE is Google.
     * 3. Returns the default search engine's URL if the DSE doesn't provide a new Tab Url.
     * @param templateUrlService The instance of {@link TemplateUrlService}.
     */
    @Nullable
    public static String getDSENewTabUrl(TemplateUrlService templateUrlService) {
        if (templateUrlService == null) {
            return ChromeSharedPreferences.getInstance()
                    .readString(ChromePreferenceKeys.DSE_NEW_TAB_URL, null);
        }

        if (templateUrlService.isDefaultSearchEngineGoogle()) return null;

        TemplateUrl templateUrl = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (templateUrl == null) return null;

        String newTabUrl = templateUrl.getNewTabURL();
        return newTabUrl != null ? newTabUrl : templateUrl.getURL();
    }

    @VisibleForTesting
    public boolean isIncognito() {
        return mProfileSupplier.hasValue() ? mProfileSupplier.get().isOffTheRecord() : false;
    }

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        if (mTemplateUrlServiceObserver == null) {
            mTemplateUrlServiceObserver = this::onTemplateURLServiceChanged;
            mTemplateUrlService.addObserver(mTemplateUrlServiceObserver);
        }
        onTemplateURLServiceChanged();
        mProfileSupplier.removeObserver(mProfileCallback);
        mProfileCallback = null;
    }

    private void onTemplateURLServiceChanged() {
        boolean isDSEGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, isDSEGoogle);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY,
                        mTemplateUrlService.isEeaChoiceCountry());
        if (isDSEGoogle) {
            ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.DSE_NEW_TAB_URL);
        } else {
            ChromeSharedPreferences.getInstance()
                    .writeString(
                            ChromePreferenceKeys.DSE_NEW_TAB_URL,
                            getDSENewTabUrl(mTemplateUrlService));
        }
    }

    private static boolean shouldSwapOutNtp() {
        return isNewTabSearchEngineUrlAndroidEnabled() && SWAP_OUT_NTP.getValue();
    }

    public TemplateUrlService getTemplateUrlServiceForTesting() {
        return mTemplateUrlService;
    }

    public static void setIsEeaChoiceCountryForTesting(boolean isEeaChoiceCountry) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY, isEeaChoiceCountry);
    }

    public static void resetIsEeaChoiceCountryForTesting() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY);
    }
}
