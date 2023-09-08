// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlUtilities;
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
     * Returns null if don't need to override.
     * @param gUrl  The GURL to check.
     * @param isIncognito Whether it is an incognito Tab.
     */
    public GURL maybeGetOverrideUrl(GURL gurl, boolean isIncognito) {
        if (isIncognito || !DseNewTabUrlManagerUtils.isNewTabSearchEngineUrlAndroidEnabled()
                || isDefaultSearchEngineGoogle() || !UrlUtilities.isNTPUrl(gurl)) {
            return gurl;
        }

        return new GURL(DseNewTabUrlManagerUtils.getDSENewTabUrl(mTemplateUrlService));
    }

    /**
     * Returns the new Tab URL of the default search engine.
     */
    @Nullable
    public String getDSENewTabUrl() {
        return DseNewTabUrlManagerUtils.getDSENewTabUrl(mTemplateUrlService);
    }

    /**
     * Returns whether the default search engine is Google. Returns a cached value if the
     * TemplateUrlService isn't ready yet.
     */
    public boolean isDefaultSearchEngineGoogle() {
        return mTemplateUrlService != null ? mTemplateUrlService.isDefaultSearchEngineGoogle()
                                           : SharedPreferencesManager.getInstance().readBoolean(
                                                   ChromePreferenceKeys.IS_DSE_GOOGLE, true);
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

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this::onTemplateURLServiceChanged);

        onTemplateURLServiceChanged();
        mProfileSupplier.removeObserver(mProfileCallback);
        mProfileCallback = null;
        mProfileSupplier = null;
    }

    private void onTemplateURLServiceChanged() {
        boolean isDSEGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_DSE_GOOGLE, isDSEGoogle);
        if (isDSEGoogle) {
            SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.DSE_NEW_TAB_URL);
        } else {
            SharedPreferencesManager.getInstance().writeString(ChromePreferenceKeys.DSE_NEW_TAB_URL,
                    DseNewTabUrlManagerUtils.getDSENewTabUrl(mTemplateUrlService));
        }
    }

    public TemplateUrlService getTemplateUrlServiceForTesting() {
        return mTemplateUrlService;
    }
}
