// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Implements {@link TabSuggestionsFetcher}. Calls a server
 * to acquire close and group suggestions for Tabs.
 */
public class TabSuggestionsServerFetcher implements TabSuggestionsFetcher {
    private static final String TAG = "TSSF";
    private static final String TAB_CONTEXT_KEY = "tabContext";
    private static final String TABS_KEY = "tabs";

    private static final String ENDPOINT = "https://memex-pa.googleapis.com/v1/suggestions";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String METHOD = "POST";
    private static final long TIMEOUT_MS = 10000L;

    private static final String ACTION_KEY = "action";
    private static final String PROVIDER_NAME_KEY = "providerName";
    private static final String SUGGESTIONS_KEY = "suggestions";
    private static final String CLOSE_KEY = "Close";
    private static final String GROUP_KEY = "Group";

    private static final String EMPTY_RESPONSE = "{}";

    private Profile mProfileForTesting;

    /**
     * Acquires Tab suggestions from an endpoint
     */
    public TabSuggestionsServerFetcher() {}

    /**
     * Constructor for testing
     * @param profile mock profile for testing purposes
     */
    @VisibleForTesting
    protected TabSuggestionsServerFetcher(Profile profile) {
        mProfileForTesting = profile;
    }

    @Override
    public void fetch(TabContext tabContext, Callback<TabSuggestionsFetcherResults> callback) {
        try {
            for (TabContext.TabInfo tabInfo : tabContext.getUngroupedTabs()) {
                if (tabInfo.isIncognito) {
                    callback.onResult(
                            new TabSuggestionsFetcherResults(Collections.emptyList(), tabContext));
                    return;
                }
            }
            EndpointFetcher.fetchUsingChromeAPIKey(res
                    -> { fetchCallback(res, callback, tabContext); },
                    mProfileForTesting == null ? Profile.getLastUsedRegularProfile()
                                               : mProfileForTesting,
                    ENDPOINT, METHOD, CONTENT_TYPE, getTabContextJson(tabContext), TIMEOUT_MS,
                    new String[] {});
        } catch (JSONException e) {
            // Soft failure for now so we don't crash the app and fall back on client side
            // providers.
            Log.e(TAG, "There was a problem parsing the JSON" + e.getMessage());
            callback.onResult(
                    new TabSuggestionsFetcherResults(Collections.emptyList(), tabContext));
        }
    }

    @VisibleForTesting
    protected String getTabContextJson(TabContext tabContext) throws JSONException {
        return new JSONObject()
                .put(TAB_CONTEXT_KEY,
                        new JSONObject().put(TABS_KEY, tabContext.getUngroupedTabsJson()))
                .toString();
    }

    private void fetchCallback(EndpointResponse response,
            Callback<TabSuggestionsFetcherResults> callback, TabContext tabContext) {
        List<TabSuggestion> suggestions = new LinkedList<>();
        JSONObject jsonResponse;
        try {
            jsonResponse = new JSONObject(response.getResponseString());
            if (jsonResponse.isNull(SUGGESTIONS_KEY)) {
                callback.onResult(
                        new TabSuggestionsFetcherResults(Collections.emptyList(), tabContext));
                return;
            }
            JSONArray jsonSuggestions = jsonResponse.getJSONArray(SUGGESTIONS_KEY);
            for (int i = 0; i < jsonResponse.length(); i++) {
                JSONObject jsonSuggestion = jsonSuggestions.getJSONObject(i);
                String jsonTabsString = jsonSuggestion.getJSONArray(TABS_KEY).toString();
                suggestions.add(new TabSuggestion(TabContext.getTabInfoFromJson(jsonTabsString),
                        getTabSuggestionAction((String) jsonSuggestion.get(ACTION_KEY)),
                        (String) jsonSuggestion.get(PROVIDER_NAME_KEY)));
            }
        } catch (JSONException e) {
            // Soft failure - fall back to client side providers
            Log.e(TAG,
                    String.format(
                            "There was a problem parsing the JSON\n Details: %s", e.getMessage()));
        }
        callback.onResult(new TabSuggestionsFetcherResults(suggestions, tabContext));
    }

    private static int getTabSuggestionAction(String action) {
        switch (action) {
            case GROUP_KEY:
                return TabSuggestion.TabSuggestionAction.GROUP;
            case CLOSE_KEY:
                return TabSuggestion.TabSuggestionAction.CLOSE;
            default:
                Log.e(TAG, String.format("Unknown action: %s\n", action));
                return -1;
        }
    }

    @Override
    public boolean isEnabled() {
        // TODO(crbug.com/1141722): Currently this Fetcher is only used for grouping suggestion,
        //  avoid fetching server if the TabGroupsAndroid flag is disabled. We need to move this
        //  flag checking logic to somewhere if this server fetcher supports suggestions other than
        //  grouping in the future.
        return isSignedIn() && isServerFetcherFlagEnabled()
                && TabUiFeatureUtilities.isTabGroupsAndroidEnabled();
    }

    @VisibleForTesting
    protected boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount();
    }

    @VisibleForTesting
    protected boolean isServerFetcherFlagEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS, "server_fetcher_enabled", false);
    }
}
