// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * A controller that listens to
 * {@link TabGroupModelFilter.Observer#didCreateGroup(List, List, boolean)} and shows a
 * undo snackbar.
 */
public class UndoGroupSnackbarController implements SnackbarManager.SnackbarController {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final SnackbarManager.SnackbarManageable mSnackbarManageable;
    private final TabGroupModelFilter.Observer mTabGroupModelFilterObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    private class TabUndoInfo {
        public final Tab tab;
        public final int tabOriginalIndex;
        public final int tabOriginalGroupId;

        TabUndoInfo(Tab tab, int tabIndex, int tabGroupId) {
            this.tab = tab;
            this.tabOriginalIndex = tabIndex;
            this.tabOriginalGroupId = tabGroupId;
        }
    }

    public UndoGroupSnackbarController(Context context, TabModelSelector tabModelSelector,
            SnackbarManager.SnackbarManageable snackbarManageable) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mSnackbarManageable = snackbarManageable;
        mTabGroupModelFilterObserver = new EmptyTabGroupModelFilterObserver() {
            @Override
            public void didCreateGroup(
                    List<Tab> tabs, List<Integer> tabOriginalIndex, boolean isSameGroup) {
                assert tabs.size() == tabOriginalIndex.size();

                List<TabUndoInfo> tabUndoInfo = new ArrayList<>();
                for (int i = 0; i < tabs.size(); i++) {
                    Tab tab = tabs.get(i);
                    int index = tabOriginalIndex.get(i);
                    int groupId = isSameGroup ? tabs.get(0).getId() : tab.getId();

                    tabUndoInfo.add(new TabUndoInfo(tab, index, groupId));
                }
                showUndoGroupSnackbar(tabUndoInfo);
            }
        };

        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 false))
                .addTabGroupObserver(mTabGroupModelFilterObserver);
        ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                 true))
                .addTabGroupObserver(mTabGroupModelFilterObserver);

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mSnackbarManageable.getSnackbarManager().dismissSnackbars(
                        UndoGroupSnackbarController.this);
            }
        };

        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    /**
     * Cleans up this class, removes {@link TabModelSelectorObserver} from {@link TabModelSelector}
     * and {@link TabGroupModelFilter.Observer} from {@link TabGroupModelFilter}.
     */
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     false))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     true))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    private void showUndoGroupSnackbar(List<TabUndoInfo> tabUndoInfo) {
        String content = String.format(Locale.getDefault(), "%d", tabUndoInfo.size());
        mSnackbarManageable.getSnackbarManager().showSnackbar(
                Snackbar.make(content, this, Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TAB_GROUP_MANUAL_CREATION_UNDO)
                        .setTemplateText(mContext.getString(R.string.undo_bar_group_tabs_message))
                        .setAction(mContext.getString(R.string.undo), tabUndoInfo));
    }

    @Override
    public void onAction(Object actionData) {
        undo((List<TabUndoInfo>) actionData);
    }

    private void undo(List<TabUndoInfo> data) {
        assert data.size() != 0;

        TabGroupModelFilter tabGroupModelFilter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        for (int i = data.size() - 1; i >= 0; i--) {
            TabUndoInfo info = data.get(i);
            tabGroupModelFilter.undoGroupedTab(
                    info.tab, info.tabOriginalIndex, info.tabOriginalGroupId);
        }
    }
}
