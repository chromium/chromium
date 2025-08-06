// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.new_tab_url;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;

/**
 * A central class for feature NewTabSearchEngineUrlAndroid which swaps out NTP if the default
 * search engine isn't Google. It holds a reference of {@link TemplateUrlService} and observes the
 * DSE changes to update the cached values in the SharedPreference.
 */
@NullMarked
public class DseNewTabUrlManager {
    private ObservableSupplier<Profile> mProfileSupplier;
    private @Nullable Callback<Profile> mProfileCallback;
    private @MonotonicNonNull RegionalCapabilitiesService mRegionalCapabilities;
    private @MonotonicNonNull TemplateUrlService mTemplateUrlService;
    private @MonotonicNonNull TemplateUrlServiceObserver mTemplateUrlServiceObserver;

    public DseNewTabUrlManager(ObservableSupplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;
        mProfileCallback = this::onProfileAvailable;
        mProfileSupplier.addObserver(mProfileCallback);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        mRegionalCapabilities = null;
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

    /** Returns whether the feature NewTabSearchEngineUrlAndroid is enabled. */
    public static boolean isNewTabSearchEngineUrlAndroidEnabled() {
        return true;
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
    public static @Nullable String getDSENewTabUrl(TemplateUrlService templateUrlService) {
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
    void onProfileAvailable(Profile profile) {
        mRegionalCapabilities = RegionalCapabilitiesServiceFactory.getForProfile(profile);
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        if (mTemplateUrlServiceObserver == null) {
            mTemplateUrlServiceObserver = this::onTemplateURLServiceChanged;
            mTemplateUrlService.addObserver(mTemplateUrlServiceObserver);
        }
        onTemplateURLServiceChanged();
        assumeNonNull(mProfileCallback);
        mProfileSupplier.removeObserver(mProfileCallback);
        mProfileCallback = null;
    }

    private void onTemplateURLServiceChanged() {
        assumeNonNull(mRegionalCapabilities);
        assumeNonNull(mTemplateUrlService);
        boolean isDSEGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, isDSEGoogle);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY,
                        mRegionalCapabilities.isInEeaCountry());
        if (isDSEGoogle) {
            ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.DSE_NEW_TAB_URL);
        } else {
            ChromeSharedPreferences.getInstance()
                    .writeString(
                            ChromePreferenceKeys.DSE_NEW_TAB_URL,
                            getDSENewTabUrl(mTemplateUrlService));
        }
    }

    public @Nullable TemplateUrlService getTemplateUrlServiceForTesting() {
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
