// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserData;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Helper class for the search suggestion module which shows search suggestions on NTP. */
public class SearchResumptionUserData implements UserData {
    // The cached search suggestion results.
    static class SuggestionResult {
        private GURL mLastUrlToTrack;
        private String[] mSuggestionTexts;
        private GURL[] mSuggestionUrls;
        private List<AutocompleteMatch> mSuggestions;

        SuggestionResult(GURL gurl, String[] suggestionTexts, GURL[] suggestionUrls) {
            mLastUrlToTrack = gurl;
            mSuggestionTexts = suggestionTexts;
            mSuggestionUrls = suggestionUrls;
            mSuggestions = null;
        }

        public SuggestionResult(GURL urlToTrack, List<AutocompleteMatch> suggestions) {
            mLastUrlToTrack = urlToTrack;
            mSuggestions = suggestions;
            mSuggestionTexts = null;
            mSuggestionUrls = null;
        }

        GURL getLastUrlToTrack() {
            return mLastUrlToTrack;
        }

        String[] getSuggestionTexts() {
            return mSuggestionTexts;
        }

        GURL[] getSuggestionUrls() {
            return mSuggestionUrls;
        }

        List<AutocompleteMatch> getSuggestions() {
            return mSuggestions;
        }
    }

    private static final Class<SearchResumptionUserData> USER_DATA_KEY =
            SearchResumptionUserData.class;
    private long mTimeStampMs = -1;
    private SuggestionResult mCachedSuggestions;

    private static SearchResumptionUserData sInstance = new SearchResumptionUserData();

    /** Gets the singleton instance for the SearchResumptionUserData. */
    public static SearchResumptionUserData getInstance() {
        return sInstance;
    }

    public static void setInstanceForTesting(SearchResumptionUserData value) {
        var prevValue = sInstance;
        sInstance = value;
        ResettersForTesting.register(() -> sInstance = prevValue);
    }

    /**
     * Caches the fetched search suggestions.
     * @param tab: The tab which the suggestions live.
     * @param urlToTrack: The URL which the search suggestions are coming from.
     * @param suggestionTexts: The content texts of suggestions.
     * @param suggestionUrls: The URLs of the suggestions.
     */
    public void cacheSuggestions(
            Tab tab, GURL urlToTrack, String[] suggestionTexts, GURL[] suggestionUrls) {
        if (tab == null || !UrlUtilities.isNtpUrl(tab.getUrl())) return;

        SearchResumptionUserData searchResumptionUserData = get(tab);
        if (searchResumptionUserData == null) {
            searchResumptionUserData = new SearchResumptionUserData();
        }
        searchResumptionUserData.mTimeStampMs = System.currentTimeMillis();
        searchResumptionUserData.mCachedSuggestions =
                new SuggestionResult(urlToTrack, suggestionTexts, suggestionUrls);
        tab.getUserDataHost().setUserData(USER_DATA_KEY, searchResumptionUserData);
    }

    /**
     * Caches the fetched search suggestions.
     * @param tab The tab which the suggestions live.
     * @param urlToTrack: The URL which the search suggestions are coming from.
     * @param suggestions: The suggestions fetched using Autocompelete API.
     */
    public void cacheSuggestions(Tab tab, GURL urlToTrack, List<AutocompleteMatch> suggestions) {
        if (tab == null || !UrlUtilities.isNtpUrl(tab.getUrl())) return;

        SearchResumptionUserData searchResumptionUserData = get(tab);
        if (searchResumptionUserData == null) {
            searchResumptionUserData = new SearchResumptionUserData();
        }
        searchResumptionUserData.mTimeStampMs = System.currentTimeMillis();
        searchResumptionUserData.mCachedSuggestions = new SuggestionResult(urlToTrack, suggestions);
        tab.getUserDataHost().setUserData(USER_DATA_KEY, searchResumptionUserData);
    }

    /** Returns the last cached search suggestions, null if the UserData isn't set or expired. */
    public SuggestionResult getCachedSuggestions(Tab tab) {
        SearchResumptionUserData searchResumptionUserData = get(tab);
        return searchResumptionUserData == null
                ? null
                : searchResumptionUserData.mCachedSuggestions;
    }

    /**
     * Returns an instance of SearchResumptionUserData if exists and isn't expired, null otherwise.
     * If the data is expired, remove it.
     */
    private static SearchResumptionUserData get(Tab tab) {
        SearchResumptionUserData searchResumptionUserData =
                tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (searchResumptionUserData == null) return null;

        assert searchResumptionUserData.mTimeStampMs != -1;
        if (searchResumptionUserData.isCachedResultExpired()) {
            tab.getUserDataHost().removeUserData(USER_DATA_KEY);
            return null;
        }
        return searchResumptionUserData;
    }

    /**
     * @return whether the cached results has expired.
     */
    private boolean isCachedResultExpired() {
        return TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis() - mTimeStampMs)
                >= ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM,
                        SearchResumptionModuleUtils.LAST_TAB_EXPIRATION_TIME_SECONDS);
    }
}
