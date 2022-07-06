// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * This is a utility class for search resumption module.
 */
public class SearchResumptionModuleUtils {
    @VisibleForTesting
    static final String TAB_EXPIRATION_TIME_PARAM = "tab_expiration_time";
    static final String USE_NEW_SERVICE_PARAM = "use_new_service";
    private static final int LAST_TAB_EXPIRATION_TIME_SECONDS = 3600; // 1 Hour

    /**
     * Creates a {@link SearchResumptionModuleCoordinator} if we are currently allowed to and
     * dependencies are met:
     * 1) Feature flags SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID and SEARCH_RESUMPTION_MODULE_ANDROID are
     *    both enabled;
     * 2) The default search engine is Google;
     * 3) The user has signed in;
     * 4) The current Tab isn't shown due to such as tapping the back button.
     * 5) The Tab to track is a regular Tab, not a native page, not with an empty URL;
     * 6) The Tab to track was visited within an expiration time.
     *
     * @param parent The parent layout which the search resumption module lives.
     * @param tabModel The TabModel to find the Tab to track.
     * @param currentTab The Tab that the search resumption module is associated to.
     * @param profile The profile of the user.
     * @param moduleContainerStubId The id of the search resumption module on its parent view.
     */
    public static SearchResumptionModuleCoordinator mayCreateSearchResumptionModule(
            ViewGroup parent, TabModel tabModel, Tab currentTab, Profile profile,
            int moduleContainerStubId) {
        if (!shouldShowSearchResumptionModule(profile, currentTab)) return null;

        Tab tabToTrack = TabModelUtils.getMostRecentTab(tabModel, currentTab.getId());
        if (!isTabToTrackValid(tabToTrack)) return null;

        return new SearchResumptionModuleCoordinator(
                parent, tabToTrack, currentTab, profile, moduleContainerStubId);
    }

    /**
     * Returns whether to show the search resumption module. Only shows the module if all of the
     * criteria meet:
     * 1) Feature flags SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID and SEARCH_RESUMPTION_MODULE_ANDROID are
     *    both enabled;
     * 2) The default search engine is Google;
     * 3) The user has signed in;
     * 4) The current Tab isn't shown due to such as tapping the back button.
     */
    @VisibleForTesting
    static boolean shouldShowSearchResumptionModule(Profile profile, Tab currentTab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID)
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID)) {
            return false;
        }

        if (!TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()
                || !IdentityServicesProvider.get().getIdentityManager(profile).hasPrimaryAccount(
                        ConsentLevel.SYNC)) {
            return false;
        }

        // If the currentTab is shown due to such as tapping the back button, we don't show the
        // search resumption module again.
        if (currentTab.canGoForward()) return false;

        return true;
    }

    /**
     * Returns whether the Tab to track is valid for providing search suggestions:
     * 1) The Tab to track is a regular Tab, not a native page, not with an empty URL;
     * 2) The Tab to track was visited within an expiration time.
     */
    @VisibleForTesting
    static boolean isTabToTrackValid(Tab tabToTrack) {
        if (tabToTrack.isNativePage() || tabToTrack.isIncognito()
                || GURL.isEmptyOrInvalid(tabToTrack.getUrl())) {
            return false;
        }

        // Only shows the module if the Tab to track was visited within an expiration time.
        return TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()
                       - CriticalPersistedTabData.from(tabToTrack).getTimestampMillis())
                < ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        TAB_EXPIRATION_TIME_PARAM, LAST_TAB_EXPIRATION_TIME_SECONDS);
    }
}
