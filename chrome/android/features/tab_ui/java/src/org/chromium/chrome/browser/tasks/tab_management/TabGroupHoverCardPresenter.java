// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Presenter that resolves tab group model data and supplies formatted primitives to {@link
 * TabGroupHoverCardView}.
 */
@NullMarked
public class TabGroupHoverCardPresenter {
    private static final String BULLET_PREFIX = "• ";

    private final TabModelSelector mTabModelSelector;

    /**
     * Constructs a presenter for tab group hover cards.
     *
     * @param tabModelSelector The {@link TabModelSelector} to query tab group state from.
     */
    public TabGroupHoverCardPresenter(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /**
     * Resolves group metadata and binds the data to the hover card view.
     *
     * @param hoverCardView The {@link TabGroupHoverCardView} to populate.
     * @param groupHeaderTabId The tab ID of the group header.
     * @param tabGroupId The stable tab group ID (Token).
     * @return True if data was successfully bound, false if group is invalid.
     */
    public boolean bindData(
            TabGroupHoverCardView hoverCardView, int groupHeaderTabId, @Nullable Token tabGroupId) {
        TabModel currentModel = mTabModelSelector.getCurrentModel();
        boolean isIncognito = currentModel.isIncognitoBranded();

        Token resolvedGroupId = tabGroupId;
        if (resolvedGroupId == null && groupHeaderTabId != Tab.INVALID_TAB_ID) {
            Tab headerTab = currentModel.getTabById(groupHeaderTabId);
            if (headerTab != null) {
                resolvedGroupId = headerTab.getTabGroupId();
            }
        }
        if (resolvedGroupId == null) {
            return false;
        }

        List<Tab> rawTabs = currentModel.getTabsInGroup(resolvedGroupId);
        if (rawTabs.isEmpty()) {
            return false;
        }

        List<Tab> relatedTabs = new ArrayList<>(rawTabs.size());
        for (Tab tab : rawTabs) {
            if (!tab.isClosing() && !tab.isDestroyed()) {
                relatedTabs.add(tab);
            }
        }
        if (relatedTabs.isEmpty()) {
            return false;
        }
        int totalTabsCount = relatedTabs.size();

        // Group Title.
        String title = currentModel.getTabGroupTitle(resolvedGroupId);
        if (TextUtils.isEmpty(title)) {
            title = TabGroupTitleUtils.getDefaultTitle(hoverCardView.getContext(), totalTabsCount);
        }

        // Child Tab Titles (up to MAX_PREVIEW_TABS).
        int previewCount = Math.min(totalTabsCount, TabGroupHoverCardView.MAX_PREVIEW_TABS);
        List<String> childTabTitles = new ArrayList<>(previewCount);
        for (int i = 0; i < previewCount; i++) {
            Tab childTab = relatedTabs.get(i);
            childTabTitles.add(BULLET_PREFIX + childTab.getTitle());
        }

        // Excess Tabs Counter.
        int excessCount = totalTabsCount - previewCount;

        hoverCardView.bindData(title, childTabTitles, excessCount, isIncognito);
        return true;
    }
}
