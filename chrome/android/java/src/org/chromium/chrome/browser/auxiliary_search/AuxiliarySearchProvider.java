// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class provides information for the auxiliary search.
 */
public class AuxiliarySearchProvider {
    private static final int kNumTabsToSend = 100;

    private final AuxiliarySearchBridge mAuxiliarySearchBridge;
    private final TabModelSelector mTabModelSelector;

    public AuxiliarySearchProvider(Profile profile, TabModelSelector tabModelSelector) {
        mAuxiliarySearchBridge = new AuxiliarySearchBridge(profile);
        mTabModelSelector = tabModelSelector;
    }

    /**
     * @return AuxiliarySearchGroup for bookmarks.
     */
    public AuxiliarySearchBookmarkGroup getBookmarksSearchableDataProto() {
        return mAuxiliarySearchBridge.getBookmarksSearchableData();
    }

    /**
     * @return AuxiliarySearchGroup for {@link Tab}s.
     */
    public AuxiliarySearchTabGroup getTabsSearchableDataProto() {
        TabList tabList = mTabModelSelector.getModel(false).getComprehensiveModel();

        // Find the the bottom of tabs in the tab switcher view if the number of the tabs more than
        // 'kNumTabsToSend'. In the multiwindow mode, the order of the 'tabList' is one window's
        // tabs, and then another's.
        int firstTabIndex = Math.max(tabList.getCount() - kNumTabsToSend, 0);
        int end = tabList.getCount() - 1;
        List<Tab> listTab = new ArrayList<>();
        for (int i = firstTabIndex; i <= end; i++) {
            listTab.add(tabList.getTabAt(i));
        }

        // Send tabs to native to filter the tabs.
        List<Tab> filteredTabs = mAuxiliarySearchBridge.getSearchableTabs(listTab);
        var tabGroupBuilder = AuxiliarySearchTabGroup.newBuilder();
        for (Tab tab : filteredTabs) {
            AuxiliarySearchEntry entry = tabToAuxiliarySearchEntry(tab);
            if (entry != null) {
                tabGroupBuilder.addTab(entry);
            }
        }
        return tabGroupBuilder.build();
    }

    /**
     * @param callback {@link Callback} to pass back the AuxiliarySearchGroup for {@link Tab}s.
     */
    public void getTabsSearchableDataProtoAsync(Callback<AuxiliarySearchTabGroup> callback) {
        TabList tabList = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> listTab = new ArrayList<>();

        for (int i = 0; i < tabList.getCount(); i++) {
            listTab.add(tabList.getTabAt(i));
        }
        mAuxiliarySearchBridge.getNonSensitiveTabs(listTab, new Callback<List<Tab>>() {
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

    private static @Nullable AuxiliarySearchEntry tabToAuxiliarySearchEntry(Tab tab) {
        String title = tab.getTitle();
        GURL url = tab.getUrl();
        if (TextUtils.isEmpty(title) || url == null || !url.isValid()) return null;

        var tabBuilder = AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url.getSpec());
        final long lastAccessTime = CriticalPersistedTabData.from(tab).getTimestampMillis();
        if (lastAccessTime != CriticalPersistedTabData.INVALID_TIMESTAMP) {
            tabBuilder.setLastAccessTimestamp(lastAccessTime);
        }

        return tabBuilder.build();
    }
}
