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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** This class provides information for the auxiliary search. */
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
     * @param callback {@link Callback} to pass back the AuxiliarySearchGroup for {@link Tab}s.
     */
    public void getTabsSearchableDataProtoAsync(Callback<AuxiliarySearchTabGroup> callback) {
        TabList tabList = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> listTab = new ArrayList<>();

        for (int i = 0; i < tabList.getCount(); i++) {
            listTab.add(tabList.getTabAt(i));
        }
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
}
