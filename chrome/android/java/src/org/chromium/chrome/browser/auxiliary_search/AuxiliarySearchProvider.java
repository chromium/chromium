// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.util.Pair;

import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

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
     * @return A list of titles and urls as pairs from tabs for the auxiliary search.
     */
    public List<Pair<String, String>> getTabsSearchableData() {
        List<Pair<String, String>> tabsList = new ArrayList<>();

        TabList tabList = mTabModelSelector.getModel(false).getComprehensiveModel();
        int firstTabIndex = Math.max(tabList.getCount() - kNumTabsToSend, 0);
        int end = tabList.getCount() - 1;
        // Find the the bottom of tabs in the tab switcher view if the number of the tabs more than
        // 'kNumTabsToSend'. In the multiwindow mode, the order of the 'tabList' is one window's
        // tabs, and then another's.
        for (int i = firstTabIndex; i <= end; i++) {
            Tab tab = tabList.getTabAt(i);
            tabsList.add(new Pair<>(tab.getTitle(), tab.getUrl().getSpec()));
        }
        return tabsList;
    }

    /**
     * @return A list of titles and urls as pairs from bookmarks for the auxiliary search.
     */
    public List<Pair<String, String>> getBookmarksSearchableData() {
        AuxiliarySearchBookmarkGroup group = mAuxiliarySearchBridge.getBookmarksSearchableData();

        List<Pair<String, String>> bookmarksList = new ArrayList<>();
        if (group != null) {
            for (int i = 0; i < group.getBookmarkCount(); i++) {
                AuxiliarySearchBookmarkGroup.Bookmark bookmark = group.getBookmark(i);
                bookmarksList.add(new Pair<>(bookmark.getTitle(), bookmark.getUrl()));
            }
        }
        return bookmarksList;
    }
}
