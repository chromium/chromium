// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/** This is a utility class for search resumption module. */
public class SearchResumptionModuleUtils {
    @IntDef({ModuleShowStatus.EXPANDED, ModuleShowStatus.COLLAPSED, ModuleShowStatus.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    // The ModuleShowStatus should be consistent with SearchResumptionModule.ModuleShowStatus in
    // enums.xml.
    public @interface ModuleShowStatus {
        int EXPANDED = 0;
        int COLLAPSED = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({
        ModuleNotShownReason.NOT_ENOUGH_RESULT,
        ModuleNotShownReason.FEATURE_DISABLED,
        ModuleNotShownReason.NOT_SIGN_IN,
        ModuleNotShownReason.NOT_SYNC,
        ModuleNotShownReason.DEFAULT_ENGINE_NOT_GOOGLE,
        ModuleNotShownReason.NO_TAB_TO_TRACK,
        ModuleNotShownReason.TAB_NOT_VALID,
        ModuleNotShownReason.TAB_EXPIRED,
        ModuleNotShownReason.TAB_CHANGED,
        ModuleNotShownReason.NUM_ENTRIES
    })
    // The ModuleNotShownReason should be consistent with
    // SearchResumptionModule.ModuleNotShownReason in enums.xml.
    public @interface ModuleNotShownReason {
        int NOT_ENOUGH_RESULT = 0;
        int FEATURE_DISABLED = 1;
        int NOT_SIGN_IN = 2;
        int NOT_SYNC = 3;
        int DEFAULT_ENGINE_NOT_GOOGLE = 4;
        int NO_TAB_TO_TRACK = 5;
        int TAB_NOT_VALID = 6;
        int TAB_EXPIRED = 7;
        int TAB_CHANGED = 8;
        int NUM_ENTRIES = 9;
    }

    @VisibleForTesting
    static final String UMA_MODULE_SHOW = "NewTabPage.SearchResumptionModule.Show";

    @VisibleForTesting
    static final String UMA_MODULE_NOT_SHOW = "NewTabPage.SearchResumptionModule.NotShow";

    static final String UMA_MODULE_SHOW_CACHED = "NewTabPage.SearchResumptionModule.Show.Cached";
    static final String TAB_EXPIRATION_TIME_PARAM = "tab_expiration_time";
    static final String ACTION_CLICK = "SearchResumptionModule.NTP.Click";
    static final String ACTION_COLLAPSE = "SearchResumptionModule.NTP.Collapse";
    static final String ACTION_EXPAND = "SearchResumptionModule.NTP.Expand";
    static final String USE_NEW_SERVICE_PARAM = "use_new_service";
    static final int LAST_TAB_EXPIRATION_TIME_SECONDS = 3600; // 1 Hour

    /**
     * Creates a {@link SearchResumptionModuleCoordinator} if we are currently allowed to and
     * dependencies are met:
     *
     * <ol>
     *   <li>Feature flag SEARCH_RESUMPTION_MODULE_ANDROID is enabled;
     *   <li>The default search engine is Google;
     *   <li>The user has signed in;
     *   <li>The current Tab isn't shown due to such as tapping the back button.
     *   <li>The Tab to track is a regular Tab, not a native page, not with an empty URL;
     *   <li>The Tab to track was visited within an expiration time.
     * </ol>
     *
     * @param parent The parent layout which the search resumption module lives.
     * @param tabModel The TabModel to find the Tab to track.
     * @param currentTab The Tab that the search resumption module is associated to.
     * @param profile The profile of the user.
     * @param moduleContainerStubId The id of the search resumption module on its parent view.
     */
    public static SearchResumptionModuleCoordinator mayCreateSearchResumptionModule(
            ViewGroup parent,
            TabModel tabModel,
            Tab currentTab,
            Profile profile,
            int moduleContainerStubId) {
        if (!shouldShowSearchResumptionModule(profile)) return null;

        Tab tabToTrack = TabModelUtils.getMostRecentTab(tabModel, currentTab.getId());
        if (tabToTrack == null) {
            recordModuleNotShownReason(ModuleNotShownReason.NO_TAB_TO_TRACK);
            return null;
        }

        if (!isTabToTrackValid(tabToTrack)) return null;

        return new SearchResumptionModuleCoordinator(
                parent,
                tabToTrack,
                currentTab,
                profile,
                moduleContainerStubId,
                mayGetCachedResults(currentTab, tabToTrack));
    }

    /**
     * Returns whether to show the search resumption module. Only shows the module if all of the
     * criteria meet:
     *
     * <ol>
     *   <li>Feature flag SEARCH_RESUMPTION_MODULE_ANDROID is enabled;
     *   <li>The default search engine is Google;
     *   <li>The user has signed in;
     *   <li>The user has turned on sync.
     * </ol>
     */
    @VisibleForTesting
    static boolean shouldShowSearchResumptionModule(Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID)) {
            recordModuleNotShownReason(ModuleNotShownReason.FEATURE_DISABLED);
            return false;
        }

        if (!TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle()) {
            recordModuleNotShownReason(ModuleNotShownReason.DEFAULT_ENGINE_NOT_GOOGLE);
            return false;
        }

        if (!IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            recordModuleNotShownReason(ModuleNotShownReason.NOT_SIGN_IN);
            return false;
        }

        if (!SyncServiceFactory.getForProfile(profile)
                .getSelectedTypes()
                .contains(UserSelectableType.HISTORY)) {
            recordModuleNotShownReason(ModuleNotShownReason.NOT_SYNC);
            return false;
        }
        return true;
    }

    /**
     * Returns whether the Tab to track is valid for providing search suggestions:
     * 1) The Tab to track is a regular Tab, not a native page, not with an empty URL;
     * 2) The Tab to track was visited within an expiration time.
     */
    @VisibleForTesting
    static boolean isTabToTrackValid(Tab tabToTrack) {
        if (tabToTrack.isNativePage()
                || tabToTrack.isIncognito()
                || GURL.isEmptyOrInvalid(tabToTrack.getUrl())) {
            recordModuleNotShownReason(ModuleNotShownReason.TAB_NOT_VALID);
            return false;
        }

        // Only shows the module if the Tab to track was visited within an expiration time.
        if (TimeUnit.MILLISECONDS.toSeconds(
                        System.currentTimeMillis() - tabToTrack.getTimestampMillis())
                < ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        TAB_EXPIRATION_TIME_PARAM,
                        LAST_TAB_EXPIRATION_TIME_SECONDS)) {
            return true;
        } else {
            recordModuleNotShownReason(ModuleNotShownReason.TAB_EXPIRED);
            return false;
        }
    }

    @VisibleForTesting
    static SuggestionResult mayGetCachedResults(Tab currentTab, Tab tabToTrack) {
        SuggestionResult cachedSuggestions = null;
        if (currentTab.canGoForward()) {
            // If the NTP is created due to any back operation, i.e., its Tab has navigated before,
            // only show the search resumption module if it has been shown before and the last
            // visited Tab hasn't changed. This prevents showing cached suggestions if the previous
            // tracking Tab has been deleted.
            cachedSuggestions =
                    SearchResumptionUserData.getInstance().getCachedSuggestions(currentTab);
            if (cachedSuggestions == null
                    || !TextUtils.equals(
                            cachedSuggestions.getLastUrlToTrack().getSpec(),
                            tabToTrack.getUrl().getSpec())) {
                SearchResumptionModuleUtils.recordModuleNotShownReason(
                        ModuleNotShownReason.TAB_CHANGED);
                return null;
            }
        }
        return cachedSuggestions;
    }

    /**
     * Records histogram when the search resumption module is shown.
     * @param cached: Whether cached suggestions are shown.
     */
    static void recordModuleShown(boolean cached) {
        boolean isCollapsed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP,
                                false);
        RecordHistogram.recordEnumeratedHistogram(
                cached ? UMA_MODULE_SHOW_CACHED : UMA_MODULE_SHOW,
                isCollapsed ? ModuleShowStatus.COLLAPSED : ModuleShowStatus.EXPANDED,
                ModuleShowStatus.NUM_ENTRIES);
    }

    /** Records the reason why the search resumption module is not shown. */
    static void recordModuleNotShownReason(@ModuleNotShownReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_MODULE_NOT_SHOW, reason, ModuleNotShownReason.NUM_ENTRIES);
    }
}
