// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.text.TextUtils;

import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

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
     * @return AuxiliarySearchGroup for tabs.
     */
    public AuxiliarySearchTabGroup getTabsSearchableDataProto() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_APP_INTEGRATION_SAFE_SEARCH)) {
            return mAuxiliarySearchBridge.getTabsSearchableData();
        }

        var tabGroupBuilder = AuxiliarySearchTabGroup.newBuilder();
        TabList tabList = mTabModelSelector.getModel(false).getComprehensiveModel();

        // Find the the bottom of tabs in the tab switcher view if the number of the tabs more than
        // 'kNumTabsToSend'. In the multiwindow mode, the order of the 'tabList' is one window's
        // tabs, and then another's.
        int firstTabIndex = Math.max(tabList.getCount() - kNumTabsToSend, 0);
        int end = tabList.getCount() - 1;
        for (int i = firstTabIndex; i <= end; i++) {
            Tab tab = tabList.getTabAt(i);
            String title = tab.getTitle();
            GURL url = tab.getUrl();
            if (TextUtils.isEmpty(title) || url == null || !url.isValid()) continue;

            var tabBuilder =
                    AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url.getSpec());
            final long lastAccessTime = CriticalPersistedTabData.from(tab).getTimestampMillis();
            if (lastAccessTime != CriticalPersistedTabData.INVALID_TIMESTAMP) {
                tabBuilder.setLastAccessTimestamp(lastAccessTime);
            }

            tabGroupBuilder.addTab(tabBuilder.build());
        }
        return tabGroupBuilder.build();
    }
}
