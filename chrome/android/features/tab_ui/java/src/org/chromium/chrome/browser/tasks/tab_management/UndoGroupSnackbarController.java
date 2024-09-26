// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * A controller that listens to {@link TabGroupModelFilterObserver#didCreateGroup(List, List, List)}
 * and shows a undo snackbar.
 */
public class UndoGroupSnackbarController implements SnackbarManager.SnackbarController {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final SnackbarManager mSnackbarManager;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;

    private static class TabUndoInfo {
        public final Tab tab;
        public final int tabOriginalIndex;
        public final int tabOriginalRootId;
        public final @Nullable Token tabOriginalTabGroupId;
        public final String destinationGroupTitle;
        public final int destinationGroupColorId;
        public final boolean destinationGroupTitleCollapsed;

        TabUndoInfo(
                Tab tab,
                int tabIndex,
                int rootId,
                @Nullable Token tabGroupId,
                String destinationGroupTitle,
                int destinationGroupColorId,
                boolean destinationGroupTitleCollapsed) {
            this.tab = tab;
            this.tabOriginalIndex = tabIndex;
            this.tabOriginalRootId = rootId;
            this.tabOriginalTabGroupId = tabGroupId;
            this.destinationGroupTitle = destinationGroupTitle;
            this.destinationGroupColorId = destinationGroupColorId;
            this.destinationGroupTitleCollapsed = destinationGroupTitleCollapsed;
        }
    }

    /**
     * @param context The current Android context.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param snackbarManager Manages the snackbar.
     */
    public UndoGroupSnackbarController(
            @NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull SnackbarManager snackbarManager) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mSnackbarManager = snackbarManager;
        mTabGroupModelFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                        // Fix for b/338511492 is to dismiss the snackbar if an ungroup operation
                        // happens because information that allowed the group action to be undone
                        // may no longer be usable (incorrect indices, group IDs, etc.).
                        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
                    }

                    @Override
                    public void didCreateGroup(
                            List<Tab> tabs,
                            List<Integer> tabOriginalIndex,
                            List<Integer> originalRootId,
                            List<Token> originalTabGroupId,
                            String destinationGroupTitle,
                            int destinationGroupColorId,
                            boolean destinationGroupTitleCollapsed) {
                        assert tabs.size() == tabOriginalIndex.size();

                        List<TabUndoInfo> tabUndoInfo = new ArrayList<>();
                        for (int i = 0; i < tabs.size(); i++) {
                            Tab tab = tabs.get(i);
                            int index = tabOriginalIndex.get(i);
                            int rootId = originalRootId.get(i);
                            Token tabGroupId = originalTabGroupId.get(i);

                            tabUndoInfo.add(
                                    new TabUndoInfo(
                                            tab,
                                            index,
                                            rootId,
                                            tabGroupId,
                                            destinationGroupTitle,
                                            destinationGroupColorId,
                                            destinationGroupTitleCollapsed));
                        }
                        showUndoGroupSnackbar(tabUndoInfo);
                    }
                };

        ((TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false))
                .addTabGroupObserver(mTabGroupModelFilterObserver);
        ((TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true))
                .addTabGroupObserver(mTabGroupModelFilterObserver);

        mCurrentTabModelObserver =
                (tabModel) -> {
                    mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
                };

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);

        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mTabModelSelector) {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab) {
                        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
                    }
                };
    }

    /**
     * Cleans up this class, removes {@link Callback<TabModel>} from {@link
     * TabModelSelector#getCurrentTabModelSupplier()} and {@link TabGroupModelFilterObserver} from
     * {@link TabGroupModelFilter}.
     */
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            ((TabGroupModelFilter)
                            mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
            ((TabGroupModelFilter)
                            mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true))
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
        }
        mTabModelSelectorTabModelObserver.destroy();
    }

    private void showUndoGroupSnackbar(List<TabUndoInfo> tabUndoInfo) {
        int mergedGroupSize =
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getRelatedTabIds(tabUndoInfo.get(0).tab.getId())
                        .size();

        String content = String.format(Locale.getDefault(), "%d", mergedGroupSize);
        String templateText;
        if (mergedGroupSize == 1) {
            templateText = mContext.getString(R.string.undo_bar_group_tab_message);
        } else {
            templateText = mContext.getString(R.string.undo_bar_group_tabs_message);
        }
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                content,
                                this,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TAB_GROUP_MANUAL_CREATION_UNDO)
                        .setTemplateText(templateText)
                        .setAction(mContext.getString(R.string.undo), tabUndoInfo));
    }

    @Override
    public void onAction(Object actionData) {
        undo((List<TabUndoInfo>) actionData);
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();

        // Delete the original tab group titles and colors of the merging tabs once the merge is
        // committed.
        for (TabUndoInfo info : (List<TabUndoInfo>) actionData) {
            int rootId = info.tabOriginalRootId;
            if (info.tab.getRootId() == rootId) continue;

            filter.deleteTabGroupVisualData(rootId);
        }
    }

    private void undo(List<TabUndoInfo> data) {
        assert data.size() != 0;

        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        TabUndoInfo firstInfo = data.get(0);
        int firstRootId = firstInfo.tab.getRootId();

        // The new rootID will be the destination tab group being merged to. If that destination
        // tab group had no title previously, on undo it may inherit a title from the group that
        // was merged to it, and persist when merging with other tabs later on. This check deletes
        // the group title for that rootID on undo since the destination group never had a group
        // title to begin with, and the merging tabs still have the original group title stored.
        if (firstInfo.destinationGroupTitle == null) {
            filter.deleteTabGroupTitle(firstRootId);
        }

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            // If the destination rootID previously did not have a color id associated with it since
            // it was either created from a new tab group or was originally a single tab before
            // merge, delete that color id on undo. This check deletes the group color for that
            // destination rootID, as all tabs still currently share that ID before the undo
            // operation is performed.
            if (firstInfo.destinationGroupColorId == TabGroupColorUtils.INVALID_COLOR_ID) {
                filter.deleteTabGroupColor(firstRootId);
            }
        }

        // The action of merging expands the destination group. If it was originally collapsed, we
        // need to restore that state.
        if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
            if (firstInfo.destinationGroupTitleCollapsed) {
                filter.setTabGroupCollapsed(firstRootId, true);
            }
        }

        for (int i = data.size() - 1; i >= 0; i--) {
            TabUndoInfo info = data.get(i);
            filter.undoGroupedTab(
                    info.tab,
                    info.tabOriginalIndex,
                    info.tabOriginalRootId,
                    info.tabOriginalTabGroupId);
        }
    }
}
