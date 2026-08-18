// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.ColorInt;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Presenter that resolves tab group model data and supplies formatted primitives to {@link
 * TabGroupHoverCardView}.
 */
@NullMarked
class TabGroupHoverCardPresenter {
    private static final String BULLET_PREFIX = "• ";

    private final TabModelSelector mTabModelSelector;

    /**
     * Constructs a presenter for tab group hover cards.
     *
     * @param tabModelSelector The {@link TabModelSelector} to query tab group state from.
     */
    TabGroupHoverCardPresenter(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /**
     * Resolves group metadata and displays the hover card view at the specified coordinates.
     *
     * @param hoverCardView The {@link TabGroupHoverCardView} to update and show.
     * @param groupHeaderTabId The tab ID of the group header.
     * @param tabGroupId The stable tab group ID (Token).
     * @param x The target x-coordinate in px.
     * @param y The target y-coordinate in px.
     */
    void show(
            TabGroupHoverCardView hoverCardView,
            int groupHeaderTabId,
            @Nullable Token tabGroupId,
            float x,
            float y) {
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
            hoverCardView.hide();
            return;
        }

        List<Tab> rawTabs = currentModel.getTabsInGroup(resolvedGroupId);
        if (rawTabs.isEmpty()) {
            hoverCardView.hide();
            return;
        }

        List<Tab> relatedTabs = new ArrayList<>(rawTabs.size());
        for (Tab tab : rawTabs) {
            if (!tab.isClosing() && !tab.isDestroyed()) {
                relatedTabs.add(tab);
            }
        }
        if (relatedTabs.isEmpty()) {
            hoverCardView.hide();
            return;
        }
        int totalTabsCount = relatedTabs.size();

        Context context = hoverCardView.getContext();

        // Group Title.
        // TODO(crbug.com/509226293): Show number of tabs in brackets when a custom title is set
        // instead of the default title (e.g. "My Group (3 Tabs)").
        String title = currentModel.getTabGroupTitle(resolvedGroupId);
        if (TextUtils.isEmpty(title)) {
            title = TabGroupTitleUtils.getDefaultTitle(context, totalTabsCount);
        }

        // Group Color.
        @TabGroupColorId int colorId = currentModel.getTabGroupColorWithFallback(resolvedGroupId);
        @ColorInt
        int groupColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        context, colorId, isIncognito);

        // Child Tab Titles (up to MAX_PREVIEW_TABS).
        int previewCount = Math.min(totalTabsCount, TabGroupHoverCardView.MAX_PREVIEW_TABS);
        List<String> childTabTitles = new ArrayList<>(previewCount);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < previewCount; i++) {
            Tab childTab = relatedTabs.get(i);
            sb.setLength(0);
            sb.append(BULLET_PREFIX).append(childTab.getTitle());
            childTabTitles.add(sb.toString());
        }

        // Excess Tabs Counter.
        int excessCount = totalTabsCount - previewCount;

        hoverCardView.show(title, groupColor, childTabTitles, excessCount, isIncognito, x, y);
    }
}
