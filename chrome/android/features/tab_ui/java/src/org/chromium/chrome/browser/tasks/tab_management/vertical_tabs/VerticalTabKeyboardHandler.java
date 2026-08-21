// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabKeyEventHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Handles physical keyboard events and tab reordering for Vertical Tabs. */
@NullMarked
class VerticalTabKeyboardHandler implements VerticalTabRailLayout.KeyEventListener {
    private final TabModelSelector mTabModelSelector;
    private final TabListModel mModelList;
    private final TabListModel mPinnedTabsModelList;
    private final RecyclerView mRecyclerView;
    private final RecyclerView mPinnedTabsRecyclerView;

    /**
     * Constructs a {@link VerticalTabKeyboardHandler}.
     *
     * @param tabModelSelector The {@link TabModelSelector} for accessing tab models.
     * @param modelList The {@link TabListModel} for unpinned tabs.
     * @param pinnedTabsModelList The {@link TabListModel} for pinned tabs.
     * @param recyclerView The {@link RecyclerView} displaying unpinned tabs.
     * @param pinnedTabsRecyclerView The {@link RecyclerView} displaying pinned tabs.
     */
    VerticalTabKeyboardHandler(
            TabModelSelector tabModelSelector,
            TabListModel modelList,
            TabListModel pinnedTabsModelList,
            RecyclerView recyclerView,
            RecyclerView pinnedTabsRecyclerView) {
        mTabModelSelector = tabModelSelector;
        mModelList = modelList;
        mPinnedTabsModelList = pinnedTabsModelList;
        mRecyclerView = recyclerView;
        mPinnedTabsRecyclerView = pinnedTabsRecyclerView;
    }

    @Override
    public boolean onKeyEvent(KeyEvent event) {
        if (TabKeyEventHandler.isCtrlDpadReorderEvent(event)) {
            if (!mRecyclerView.hasFocus() && !mPinnedTabsRecyclerView.hasFocus()) {
                return false;
            }
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                return reorderKeyboardFocusedItem(TabKeyEventHandler.isMovePrevious(event));
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                return true;
            }
        }
        return false;
    }

    /**
     * Reorders the currently keyboard-focused item in Vertical Tabs.
     *
     * @param toPrevious Whether the item should be reordered to previous (up) or next (down).
     * @return Whether the item was successfully reordered.
     */
    boolean reorderKeyboardFocusedItem(boolean toPrevious) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null) return false;

        if (mPinnedTabsRecyclerView.hasFocus()) {
            return reorderFocusedChild(
                    mPinnedTabsRecyclerView,
                    mPinnedTabsModelList,
                    tabModel,
                    /* moveSingleTabOverride= */ true,
                    toPrevious);
        }
        if (mRecyclerView.hasFocus()) {
            return reorderFocusedChild(
                    mRecyclerView,
                    mModelList,
                    tabModel,
                    /* moveSingleTabOverride= */ null,
                    toPrevious);
        }
        return false;
    }

    private boolean reorderFocusedChild(
            RecyclerView recyclerView,
            TabListModel modelList,
            TabModel tabModel,
            @Nullable Boolean moveSingleTabOverride,
            boolean toPrevious) {
        View focus = recyclerView.findFocus();
        if (focus == null) return false;
        View focusedChild = recyclerView.findContainingItemView(focus);
        if (focusedChild == null) return false;
        int pos = recyclerView.getChildAdapterPosition(focusedChild);
        if (pos == RecyclerView.NO_POSITION || pos >= modelList.size()) return false;
        if (toPrevious && pos == 0) return false;
        if (!toPrevious && pos == modelList.size() - 1) return false;

        PropertyModel model = modelList.get(pos).model;
        @TabId int tabId = TabProperties.getTabId(model);
        if (tabId == Tab.INVALID_TAB_ID) return false;

        boolean moveSingleTab =
                moveSingleTabOverride != null
                        ? moveSingleTabOverride
                        : !TabProperties.isTabGroupHeader(model);

        TabKeyEventHandler.reorderTab(
                tabModel, tabId, /* moveForward= */ toPrevious, moveSingleTab);
        return true;
    }
}
