// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.WeakHashMap;

/**
 * Representation of a Tab-like card in the Grid Tab Switcher.
 */
public class PseudoTab {
    private final Integer mTabId;
    private final WeakReference<Tab> mTab;
    private static final WeakHashMap<Integer, PseudoTab> sAllTabs = new WeakHashMap<>();

    /**
     * An interface to get the title to be used for a tab.
     */
    public interface TitleProvider { String getTitle(PseudoTab tab); }

    /**
     * Construct from a tab ID. An earlier instance with the same ID can be returned.
     */
    public static PseudoTab fromTabId(int tabId) {
        PseudoTab cached = sAllTabs.get(tabId);
        if (cached != null) return cached;
        return new PseudoTab(tabId);
    }

    private PseudoTab(int tabId) {
        mTabId = tabId;
        mTab = null;
        sAllTabs.put(getId(), this);
    }

    /**
     * Construct from a {@link Tab}. An earlier instance with the same {@link Tab} can be returned.
     */
    public static PseudoTab fromTab(@NonNull Tab tab) {
        PseudoTab cached = sAllTabs.get(tab.getId());
        if (cached != null && cached.hasRealTab()) {
            assert cached.getTab() == tab;
            return cached;
        }
        // We need to upgrade a pre-native Tab to a post-native Tab.
        return new PseudoTab(tab);
    }

    private PseudoTab(@NonNull Tab tab) {
        mTabId = tab.getId();
        mTab = new WeakReference<>(tab);
        sAllTabs.put(getId(), this);
    }

    /**
     * Convert a list of {@link Tab} to a list of {@link PseudoTab}.
     * @param tabs A list of {@link Tab}
     * @return A list of {@link PseudoTab}
     */
    public static List<PseudoTab> getListOfPseudoTab(@Nullable List<Tab> tabs) {
        List<PseudoTab> pseudoTabs = null;
        if (tabs != null) {
            pseudoTabs = new ArrayList<>();
            for (Tab tab : tabs) {
                pseudoTabs.add(fromTab(tab));
            }
        }
        return pseudoTabs;
    }

    /**
     * Convert a {@link TabList} to a list of {@link PseudoTab}.
     * @param tabList A {@link TabList}
     * @return A list of {@link PseudoTab}
     */
    public static List<PseudoTab> getListOfPseudoTab(@Nullable TabList tabList) {
        List<PseudoTab> pseudoTabs = null;
        if (tabList != null) {
            pseudoTabs = new ArrayList<>();
            for (int i = 0; i < tabList.getCount(); i++) {
                pseudoTabs.add(fromTab(tabList.getTabAt(i)));
            }
        }
        return pseudoTabs;
    }

    @Override
    public String toString() {
        assert mTabId != null;
        return "Tab " + mTabId;
    }

    /**
     * @return The ID of the {@link PseudoTab}
     */
    public int getId() {
        return mTabId;
    }

    /**
     * Get the title of the {@link PseudoTab} through a {@link TitleProvider}.
     *
     * If the {@link TitleProvider} is {@code null}, fall back to {@link #getTitle()}.
     * @param titleProvider The {@link TitleProvider} to provide the title.
     * @return The title
     */
    public String getTitle(@Nullable TitleProvider titleProvider) {
        if (titleProvider != null) return titleProvider.getTitle(this);
        return getTitle();
    }

    /**
     * Get the title of the {@link PseudoTab}.
     * @return The title
     */
    public String getTitle() {
        if (mTab != null && mTab.get() != null) {
            return mTab.get().getTitle();
        }
        assert mTabId != null;
        return TabAttributeCache.getTitle(mTabId);
    }

    /**
     * Get the URL of the {@link PseudoTab}.
     * @return The URL
     */
    public String getUrl() {
        if (mTab != null && mTab.get() != null) {
            return mTab.get().getUrl();
        }
        assert mTabId != null;
        return TabAttributeCache.getUrl(mTabId);
    }

    /**
     * Get the root ID of the {@link PseudoTab}.
     * @return The root ID
     */
    public int getRootId() {
        if (mTab != null && mTab.get() != null) {
            return mTab.get().getRootId();
        }
        assert mTabId != null;
        return TabAttributeCache.getRootId(mTabId);
    }

    /**
     * @return Whether the {@link PseudoTab} is in the Incognito mode.
     */
    public boolean isIncognito() {
        if (mTab != null && mTab.get() != null) return mTab.get().isIncognito();
        assert mTabId != null;
        return false;
    }

    /**
     * @return {@link Tab#getTimestampMillis()} of the underlying real {@link Tab}
     */
    public long getTimestampMillis() {
        assert mTab != null
                && mTab.get() != null : "getTimestampMillis can only be used with real tabs";
        return mTab.get().getTimestampMillis();
    }

    /**
     * @return Whether an underlying real {@link Tab} is available.
     */
    public boolean hasRealTab() {
        return getTab() != null;
    }

    /**
     * Get the underlying real {@link Tab}. We should avoid using this.
     * @return The underlying real {@link Tab}.
     */
    @Deprecated
    public @Nullable Tab getTab() {
        if (mTab != null) return mTab.get();
        return null;
    }

    /**
     * Get related tabs of a certain {@link PseudoTab}, through {@link TabModelFilter}s if
     * available.
     * @param member The {@link PseudoTab} related to
     * @param provider The {@link TabModelFilterProvider} to query the tab relation
     * @return Related {@link PseudoTab}s
     */
    public static @NonNull List<PseudoTab> getRelatedTabs(
            PseudoTab member, @NonNull TabModelFilterProvider provider) {
        List<Tab> relatedTabs = getRelatedTabList(provider, member.getId());
        if (relatedTabs != null) return getListOfPseudoTab(relatedTabs);

        List<PseudoTab> related = new ArrayList<>();
        int rootId = member.getRootId();
        if (rootId == Tab.INVALID_TAB_ID || !FeatureUtilities.isTabGroupsAndroidEnabled()) {
            related.add(member);
            return related;
        }
        for (Integer key : sAllTabs.keySet()) {
            PseudoTab tab = sAllTabs.get(key);
            assert tab != null;
            if (tab.getRootId() == Tab.INVALID_TAB_ID) continue;
            if (tab.getRootId() != rootId) continue;
            related.add(tab);
        }
        assert related.size() > 0;
        return related;
    }

    private static @Nullable List<Tab> getRelatedTabList(
            @NonNull TabModelFilterProvider provider, int tabId) {
        if (provider.getTabModelFilter(false) != null) {
            List<Tab> related = provider.getTabModelFilter(false).getRelatedTabList(tabId);
            if (related.size() > 0) return related;
        }
        if (provider.getTabModelFilter(true) != null) {
            List<Tab> related = provider.getTabModelFilter(true).getRelatedTabList(tabId);
            assert related.size() > 0;
            return related;
        }
        return null;
    }
}
