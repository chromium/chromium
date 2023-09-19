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
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
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
        if (isIncognito() || !isNewTabSearchEngineUrlAndroidEnabled()
                || isDefaultSearchEngineGoogle() || !UrlUtilities.isNTPUrl(gurl)) {
            return gurl;
        }

        String newTabUrl = getDSENewTabUrl(mTemplateUrlService);
        return newTabUrl != null ? new GURL(newTabUrl) : gurl;
    }

    /**
     * Returns the new Tab URL of the default search engine.
     */
    @Nullable
    public String getDSENewTabUrl() {
        return getDSENewTabUrl(mTemplateUrlService);
    }

    public void destroy() {
        if (mProfileSupplier != null && mProfileCallback != null) {
            mProfileSupplier.removeObserver(mProfileCallback);
            mProfileCallback = null;
            mProfileSupplier = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this::onTemplateURLServiceChanged);
            mTemplateUrlService = null;
        }
    }

    /**
     * Returns the new Tab URL of the default search engine if should override any NTP's URL.
     * Returns the given URL if don't need to override.
     * @param url The URL to check.
     * @param profile The instance of the current {@link Profile}.
     */
    public static GURL maybeGetOverrideUrl(GURL gurl, Profile profile) {
        if ((profile != null && profile.isOffTheRecord())
                || !isNewTabSearchEngineUrlAndroidEnabled() || isDefaultSearchEngineGoogle()
                || !UrlUtilities.isNTPUrl(gurl)) {
            return gurl;
        }

        TemplateUrlService templateUrlService =
                profile != null ? TemplateUrlServiceFactory.getForProfile(profile) : null;
        String newTabUrl = getDSENewTabUrl(templateUrlService);
        return newTabUrl != null ? new GURL(newTabUrl) : gurl;
    }

    /**
     * Returns whether the feature NewTabSearchEngineUrlAndroid is enabled.
     */
    public static boolean isNewTabSearchEngineUrlAndroidEnabled() {
        return ChromeFeatureList.sNewTabSearchEngineUrlAndroid.isEnabled();
    }

    /**
     * Returns cached value of {@link ChromePreferenceKeys.IS_DSE_GOOGLE} in the SharedPreference.
     */
    public static boolean isDefaultSearchEngineGoogle() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.IS_DSE_GOOGLE, true);
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
            return SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.DSE_NEW_TAB_URL, null);
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
        mTemplateUrlService.addObserver(this::onTemplateURLServiceChanged);

        onTemplateURLServiceChanged();
        mProfileSupplier.removeObserver(mProfileCallback);
        mProfileCallback = null;
    }

    private void onTemplateURLServiceChanged() {
        boolean isDSEGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_DSE_GOOGLE, isDSEGoogle);
        if (isDSEGoogle) {
            SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.DSE_NEW_TAB_URL);
        } else {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.DSE_NEW_TAB_URL, getDSENewTabUrl(mTemplateUrlService));
        }
    }

    public TemplateUrlService getTemplateUrlServiceForTesting() {
        return mTemplateUrlService;
    }
}
