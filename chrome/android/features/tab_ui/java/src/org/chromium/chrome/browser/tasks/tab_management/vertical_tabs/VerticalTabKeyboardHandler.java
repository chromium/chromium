// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabKeyEventHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;

/** Handles physical keyboard events and tab reordering for Vertical Tabs. */
@NullMarked
class VerticalTabKeyboardHandler implements VerticalTabRailLayout.KeyEventListener {
    private final TabModelSelector mTabModelSelector;
    private final TabListModel mModelList;
    private final TabListModel mPinnedTabsModelList;
    private final RecyclerView mRecyclerView;
    private final RecyclerView mPinnedTabsRecyclerView;
    private final VerticalTabHoverCardController mHoverCardController;

    /**
     * Constructs a {@link VerticalTabKeyboardHandler}.
     *
     * @param tabModelSelector The {@link TabModelSelector} for accessing tab models.
     * @param modelList The {@link TabListModel} for unpinned tabs.
     * @param pinnedTabsModelList The {@link TabListModel} for pinned tabs.
     * @param recyclerView The {@link RecyclerView} displaying unpinned tabs.
     * @param pinnedTabsRecyclerView The {@link RecyclerView} displaying pinned tabs.
     * @param hoverCardController The {@link VerticalTabHoverCardController} for hover card state.
     */
    VerticalTabKeyboardHandler(
            TabModelSelector tabModelSelector,
            TabListModel modelList,
            TabListModel pinnedTabsModelList,
            RecyclerView recyclerView,
            RecyclerView pinnedTabsRecyclerView,
            VerticalTabHoverCardController hoverCardController) {
        mTabModelSelector = tabModelSelector;
        mModelList = modelList;
        mPinnedTabsModelList = pinnedTabsModelList;
        mRecyclerView = recyclerView;
        mPinnedTabsRecyclerView = pinnedTabsRecyclerView;
        mHoverCardController = hoverCardController;
    }

    @Override
    public boolean onKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
            if (mHoverCardController.isHoverCardShowing()) {
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    mHoverCardController.hideHoverCard();
                }
                return true;
            }
        }
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
    @VisibleForTesting
    boolean reorderKeyboardFocusedItem(boolean toPrevious) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null) return false;

        if (mPinnedTabsRecyclerView.hasFocus()) {
            return reorderFocusedChild(
                    mPinnedTabsRecyclerView, mPinnedTabsModelList, tabModel, toPrevious);
        }
        if (mRecyclerView.hasFocus()) {
            return reorderFocusedChild(mRecyclerView, mModelList, tabModel, toPrevious);
        }
        return false;
    }

    private boolean reorderFocusedChild(
            RecyclerView recyclerView,
            TabListModel modelList,
            TabModel tabModel,
            boolean toPrevious) {
        View focus = recyclerView.findFocus();
        if (focus == null) return false;
        View focusedChild = recyclerView.findContainingItemView(focus);
        if (focusedChild == null) return false;
        int pos = recyclerView.getChildAdapterPosition(focusedChild);
        if (pos == RecyclerView.NO_POSITION || pos >= modelList.size()) return false;
        if (toPrevious && pos == 0) return false;

        return VerticalTabReorderUtils.reorderItemInDirection(tabModel, modelList, pos, toPrevious);
    }
}
