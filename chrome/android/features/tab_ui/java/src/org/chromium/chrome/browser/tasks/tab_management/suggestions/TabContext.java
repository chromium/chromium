// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.content_public.browser.NavigationEntry;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Represents a snapshot of the current tabs and tab groups.
 */
public class TabContext {
    private static final String ID_KEY = "id";
    private static final String URL_KEY = "url";
    private static final String TITLE_KEY = "title";
    private static final String TIMESTAMP_KEY = "timestamp";
    private static final String REFERRER_KEY = "referrer";

    /**
     * Holds basic information about a tab group.
     */
    public static class TabGroupInfo {
        public final int rootId;
        public final List<TabInfo> tabs;

        public TabGroupInfo(int rootId, List<TabInfo> tabs) {
            this.rootId = rootId;
            this.tabs = Collections.unmodifiableList(tabs);
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (other instanceof TabGroupInfo) {
                TabGroupInfo otherGroupInfo = (TabGroupInfo) other;
                return rootId == otherGroupInfo.rootId && tabs == null
                        ? otherGroupInfo.tabs == null
                        : tabs.equals(otherGroupInfo.tabs);
            }
            return false;
        }

        @Override
        public int hashCode() {
            int result = 31 * (tabs == null ? 0 : tabs.hashCode());
            result = 31 * result + rootId;
            return result;
        }
    }

    /**
     * Holds basic information about a tab.
     */
    public static class TabInfo implements Comparable<TabInfo> {
        // equals() and hashCode() only include url and id
        public final String url;
        public final String referrerUrl;
        public final long timestampMillis;
        public final int id;
        public final String title;
        public final String originalUrl;
        public final String visibleUrl;
        public final boolean isIncognito;

        /**
         * Constructs a new TabInfo object
         */
        protected TabInfo(int id, String title, String url, String originalUrl, String referrerUrl,
                long timestampMillis, String visibleUrl, boolean isIncognito) {
            this.id = id;
            this.title = title;
            this.url = url;
            this.originalUrl = originalUrl;
            this.referrerUrl = referrerUrl;
            this.timestampMillis = timestampMillis;
            this.visibleUrl = visibleUrl;
            this.isIncognito = isIncognito;
        }

        /**
         * Constructs a new non-incognito TabInfo object
         */
        protected TabInfo(int id, String title, String url, String originalUrl, String referrerUrl,
                long timestampMillis, String visibleUrl) {
            this(id, title, url, originalUrl, referrerUrl, timestampMillis, visibleUrl, false);
        }

        /**
         * Creates a new TabInfo object from {@link Tab}
         */
        public static TabInfo createFromTab(Tab tab) {
            String referrerUrl = getReferrerUrlFromTab(tab);
            return new TabInfo(tab.getId(), tab.getTitle(), tab.getUrlString(),
                    tab.getOriginalUrl().getSpec(), referrerUrl != null ? referrerUrl : "",
                    CriticalPersistedTabData.from(tab).getTimestampMillis(), tab.getUrlString(),
                    tab.isIncognito());
        }

        public double getSiteEngagementScore() {
            return SiteEngagementService.getForBrowserContext(Profile.getLastUsedRegularProfile())
                    .getScore(visibleUrl);
        }

        private static String getReferrerUrlFromTab(Tab tab) {
            if (tab.getWebContents() == null) {
                return null;
            }
            if (tab.getWebContents().getNavigationController() == null) {
                return null;
            }
            int lastCommittedIndex =
                    tab.getWebContents().getNavigationController().getLastCommittedEntryIndex();
            NavigationEntry lastCommittedEntry =
                    tab.getWebContents().getNavigationController().getEntryAtIndex(
                            lastCommittedIndex);
            if (lastCommittedEntry == null) {
                return null;
            }
            return lastCommittedEntry.getReferrerUrl().getSpec();
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (other instanceof TabInfo) {
                TabInfo otherTabInfo = (TabInfo) other;
                return id == otherTabInfo.id && TextUtils.equals(url, otherTabInfo.url);
            }
            return false;
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + id;
            result = 31 * result + url == null ? 0 : url.hashCode();
            return result;
        }

        @Override
        public int compareTo(TabInfo other) {
            return Integer.compare(id, other.id);
        }
    }

    private final List<TabInfo> mUngroupedTabs;
    private final List<TabGroupInfo> mTabGroups;

    protected TabContext(List<TabInfo> ungroupedTabs, List<TabGroupInfo> groups) {
        mUngroupedTabs = Collections.unmodifiableList(ungroupedTabs);
        mTabGroups = Collections.unmodifiableList(groups);
    }

    public List<TabInfo> getUngroupedTabs() {
        return mUngroupedTabs;
    }

    public List<TabGroupInfo> getTabGroups() {
        return mTabGroups;
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (other == null) return false;
        if (other instanceof TabContext) {
            TabContext otherTabContext = (TabContext) other;
            return (mTabGroups == null ? otherTabContext.getTabGroups() == null
                                       : mTabGroups.equals(otherTabContext.getTabGroups()))
                    && (mUngroupedTabs == null
                                    ? otherTabContext.getUngroupedTabs() == null
                                    : mUngroupedTabs.equals(otherTabContext.getUngroupedTabs()));
        }
        return false;
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + (mTabGroups == null ? 0 : mTabGroups.hashCode());
        result = 31 * result + (mUngroupedTabs == null ? 0 : mUngroupedTabs.hashCode());
        return result;
    }

    /**
     * Creates an instance of TabContext based on the provided {@link TabModelSelector}.
     * @param tabModelSelector TabModelSelector for which the TabContext will be derived
     * @return an instance of TabContext
     */
    public static TabContext createCurrentContext(TabModelSelector tabModelSelector) {
        TabModelFilter tabModelFilter =
                tabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        List<TabInfo> ungroupedTabs = new ArrayList<>();
        List<TabGroupInfo> existingGroups = new ArrayList<>();

        // Examine each tab in the current model and either add it to the list of ungrouped tabs or
        // add it to a group it belongs to.
        for (int i = 0; i < tabModelFilter.getCount(); i++) {
            Tab currentTab = tabModelFilter.getTabAt(i);

            assert currentTab != null : "currentTab should not be null";

            // TODO(crbug.com/1146320): Investigate the NPE.
            if (currentTab == null) continue;

            List<Tab> relatedTabs = tabModelFilter.getRelatedTabList(currentTab.getId());

            if (relatedTabs.size() > 1) {
                List<Tab> nonClosingTabs = getNonClosingTabs(relatedTabs);
                existingGroups.add(
                        new TabGroupInfo(CriticalPersistedTabData.from(currentTab).getRootId(),
                                createTabInfoList(nonClosingTabs)));
            } else {
                if (currentTab.isClosing()) continue;
                ungroupedTabs.add(TabInfo.createFromTab(currentTab));
            }
        }

        return new TabContext(ungroupedTabs, existingGroups);
    }

    private static List<Tab> getNonClosingTabs(List<Tab> tabs) {
        List<Tab> nonClosingTabs = new ArrayList<>();
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            if (tab.isClosing()) continue;
            nonClosingTabs.add(tab);
        }
        return nonClosingTabs;
    }

    private static List<TabInfo> createTabInfoList(List<Tab> tabs) {
        List<TabInfo> tabInfoList = new ArrayList<>();
        for (Tab tab : tabs) {
            tabInfoList.add(TabInfo.createFromTab(tab));
        }
        return tabInfoList;
    }

    /**
     * @return Ungrouped tabs in a JSON Array
     * @throws JSONException if there was a problem acquiring the JSONObject
     */
    public JSONArray getUngroupedTabsJson() throws JSONException {
        JSONArray jsonTabs = new JSONArray();
        for (TabContext.TabInfo tab : getUngroupedTabs()) {
            JSONObject jsonTab = new JSONObject();
            jsonTab.put(ID_KEY, tab.id);
            jsonTab.put(URL_KEY, tab.url);
            jsonTab.put(TITLE_KEY, tab.title);
            jsonTab.put(TIMESTAMP_KEY, tab.timestampMillis);
            if (tab.referrerUrl != null) {
                jsonTab.put(REFERRER_KEY, tab.referrerUrl);
            }
            jsonTabs.put(jsonTab);
        }
        return jsonTabs;
    }

    /**
     * @param jsonString string in JSON format to be parsed
     * @return parsed list of TabInfo
     * @throws JSONException if there was a problem parsing the JSON
     */
    public static List<TabInfo> getTabInfoFromJson(String jsonString) throws JSONException {
        JSONArray jsonTabs = new JSONArray(jsonString);
        List<TabContext.TabInfo> tabs = new LinkedList<>();
        for (int j = 0; j < jsonTabs.length(); j++) {
            JSONObject jsonTab = jsonTabs.getJSONObject(j);
            tabs.add(new TabContext.TabInfo(Integer.parseInt(jsonTab.getString(ID_KEY)),
                    jsonTab.getString(TITLE_KEY), jsonTab.getString(URL_KEY), null,
                    jsonTab.getString(REFERRER_KEY),
                    Long.parseLong(jsonTab.getString(TIMESTAMP_KEY)), null,
                    false /** Only support non-incognito for now*/));
        }
        return tabs;
    }
}
