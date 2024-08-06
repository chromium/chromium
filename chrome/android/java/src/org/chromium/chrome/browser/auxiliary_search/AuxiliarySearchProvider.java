// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** This class provides information for the auxiliary search. */
public class AuxiliarySearchProvider {
    private static final int kNumTabsToSend = 100;

    /* Only donate the recent 7 days accessed tabs.*/
    @VisibleForTesting static final String TAB_AGE_HOURS_PARAM = "tabs_max_hours";
    @VisibleForTesting static final int DEFAULT_TAB_AGE_HOURS = 168;

    private final AuxiliarySearchBridge mAuxiliarySearchBridge;
    private final TabModelSelector mTabModelSelector;
    private Long mTabMaxAgeMillis;

    public AuxiliarySearchProvider(Profile profile, TabModelSelector tabModelSelector) {
        mAuxiliarySearchBridge = new AuxiliarySearchBridge(profile);
        mTabModelSelector = tabModelSelector;
        mTabMaxAgeMillis = getTabsMaxAgeMs();
    }

    /**
     * @return AuxiliarySearchGroup for bookmarks.
     */
    public AuxiliarySearchBookmarkGroup getBookmarksSearchableDataProto() {
        return mAuxiliarySearchBridge.getBookmarksSearchableData();
    }

    /**
     * @param callback {@link Callback} to pass back the AuxiliarySearchGroup for {@link Tab}s.
     */
    public void getTabsSearchableDataProtoAsync(Callback<AuxiliarySearchTabGroup> callback) {
        long minAccessTime = System.currentTimeMillis() - mTabMaxAgeMillis;
        List<Tab> listTab = getTabsByMinimalAccessTime(minAccessTime);

        mAuxiliarySearchBridge.getNonSensitiveTabs(
                listTab,
                new Callback<List<Tab>>() {
                    @Override
                    public void onResult(List<Tab> tabs) {
                        var tabGroupBuilder = AuxiliarySearchTabGroup.newBuilder();

                        for (Tab tab : tabs) {
                            AuxiliarySearchEntry entry = tabToAuxiliarySearchEntry(tab);
                            if (entry != null) {
                                tabGroupBuilder.addTab(entry);
                            }
                        }

                        callback.onResult(tabGroupBuilder.build());
                    }
                });
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static @Nullable AuxiliarySearchEntry tabToAuxiliarySearchEntry(@Nullable Tab tab) {
        if (tab == null) {
            return null;
        }

        String title = tab.getTitle();
        GURL url = tab.getUrl();
        if (TextUtils.isEmpty(title) || url == null || !url.isValid()) return null;

        var tabBuilder = AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url.getSpec());
        final long lastAccessTime = tab.getTimestampMillis();
        if (lastAccessTime != Tab.INVALID_TIMESTAMP) {
            tabBuilder.setLastAccessTimestamp(lastAccessTime);
        }

        return tabBuilder.build();
    }

    /**
     * @param minAccessTime specifies the earliest access time for a tab to be included in the
     *     returned list.
     * @return List of {@link Tab} which is accessed after 'minAccessTime'.
     */
    @VisibleForTesting
    List<Tab> getTabsByMinimalAccessTime(long minAccessTime) {
        TabList allTabs = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> recentAccessedTabs = new ArrayList<>();

        for (int i = 0; i < allTabs.getCount(); i++) {
            Tab tab = allTabs.getTabAt(i);
            if (tab.getTimestampMillis() >= minAccessTime) {
                recentAccessedTabs.add(allTabs.getTabAt(i));
            }
        }

        return recentAccessedTabs;
    }

    /** Returns the donated tab's max age in MS. */
    @VisibleForTesting
    long getTabsMaxAgeMs() {
        int configuredTabMaxAgeHrs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.ANDROID_APP_INTEGRATION, TAB_AGE_HOURS_PARAM, 0);
        if (configuredTabMaxAgeHrs == 0) configuredTabMaxAgeHrs = DEFAULT_TAB_AGE_HOURS;
        return TimeUnit.HOURS.toMillis(configuredTabMaxAgeHrs);
    }
}
