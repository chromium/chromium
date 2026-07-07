// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Mediator for Static Pinned Tabs. Observes the main TabListModel and TabModel to mirror pinned
 * tabs into a dedicated ModelList. It shares the exact same PropertyModel instances with the main
 * list so that state updates (such as titles and favicons) propagate automatically.
 */
@NullMarked
public class StaticPinnedTabsMediator {
    private @Nullable TabModel mTabModel;
    private final TabListModel mMainModelList;
    private final TabListModel mPinnedModelList;
    private final TabModelObserver mTabModelObserver;
    private final ListObservable.ListObserver<Void> mMainListObserver;
    private final Runnable mOnVisibilityChanged;
    private boolean mWasEmpty = true;

    /**
     * @param tabModel The TabModel backing the tab switcher.
     * @param mainModelList The main ModelList representing the scrollable tab list.
     * @param pinnedModelList The ModelList representing the pinned tabs strip.
     * @param onVisibilityChanged Callback triggered when the pinned tabs strip changes visibility.
     */
    public StaticPinnedTabsMediator(
            @Nullable TabModel tabModel,
            TabListModel mainModelList,
            TabListModel pinnedModelList,
            Runnable onVisibilityChanged) {
        mTabModel = tabModel;
        mMainModelList = mainModelList;
        mPinnedModelList = pinnedModelList;
        mOnVisibilityChanged = onVisibilityChanged;

        mMainListObserver =
                new ListObservable.ListObserver<>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        if (index > mPinnedModelList.size()) return;

                        for (int i = 0; i < count; i++) {
                            ListItem item = mMainModelList.get(index + i);
                            if (item.model.get(TabProperties.IS_PINNED)) {
                                addListItemToPinnedModelList(item);
                            }
                        }
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        if (mMainModelList.isEmpty()) {
                            mPinnedModelList.clear();
                            notifyVisibilityChangedIfNeeded();
                            return;
                        }

                        // Pinned tabs are contiguous at the start of the list. Any removal past
                        // that range cannot affect pinned tabs.
                        if (index >= mPinnedModelList.size()) return;

                        // Pre-compute the active tab IDs to perform constant time lookups in the
                        // loop below.
                        Set<Integer> mainTabIds = new HashSet<>();
                        for (int i = 0; i < mMainModelList.size(); i++) {
                            mainTabIds.add(mMainModelList.get(i).model.get(TabProperties.TAB_ID));
                        }

                        // Scan and remove any items from the pinned list that are no longer
                        // present in the main model list.
                        for (int i = mPinnedModelList.size() - 1; i >= 0; i--) {
                            ListItem item = mPinnedModelList.get(i);
                            int tabId = item.model.get(TabProperties.TAB_ID);
                            if (!mainTabIds.contains(tabId)) {
                                mPinnedModelList.removeAt(i);
                            }
                        }
                        notifyVisibilityChangedIfNeeded();
                    }

                    @Override
                    public void onItemRangeChanged(
                            ListObservable source, int index, int count, @Nullable Void payload) {
                        // Intentionally empty.
                    }

                    @Override
                    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                        ListItem item = mMainModelList.get(newIndex);
                        if (!item.model.get(TabProperties.IS_PINNED)) return;

                        int currentPinnedIndex = mPinnedModelList.indexOf(item);

                        // If newIndex >= mPinnedModelList.size(), it's being unpinned and will be
                        // removed by didChangePinState shortly.
                        if (currentPinnedIndex != TabModel.INVALID_TAB_INDEX
                                && newIndex >= 0
                                && newIndex < mPinnedModelList.size()) {
                            if (currentPinnedIndex != newIndex) {
                                mPinnedModelList.move(currentPinnedIndex, newIndex);
                            }
                        }
                    }
                };

        mMainModelList.addObserver(mMainListObserver);

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didChangePinState(Tab tab) {
                        if (tab.getIsPinned()) {
                            int index = mMainModelList.indexFromTabId(tab.getId());
                            if (index != TabModel.INVALID_TAB_INDEX) {
                                addListItemToPinnedModelList(mMainModelList.get(index));
                            }
                        } else {
                            removeTabFromPinnedModelList(tab.getId());
                        }
                    }

                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, int closingSource) {
                        for (Tab tab : tabs) {
                            removeTabFromPinnedModelList(tab.getId());
                        }
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        removeTabFromPinnedModelList(tab.getId());
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        removeTabFromPinnedModelList(tab.getId());
                    }
                };

        if (mTabModel != null) {
            mTabModel.addObserver(mTabModelObserver);
        }

        updatePinnedTabsList();
    }

    /** Destroys this mediator and removes observers. */
    public void destroy() {
        mMainModelList.removeObserver(mMainListObserver);
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
    }

    /** Updates the TabModel used by this mediator. */
    public void updateTabModel(TabModel tabModel) {
        if (mTabModel == tabModel) return;
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
        mTabModel = tabModel;
        if (mTabModel != null) {
            mTabModel.addObserver(mTabModelObserver);
        }
        updatePinnedTabsList();
    }

    private void updatePinnedTabsList() {
        mPinnedModelList.clear();
        for (int i = 0; i < mMainModelList.size(); i++) {
            ListItem item = mMainModelList.get(i);
            if (item.model.get(TabProperties.IS_PINNED)) {
                mPinnedModelList.add(item);
            }
        }
        notifyVisibilityChangedIfNeeded();
    }

    private void addListItemToPinnedModelList(ListItem item) {
        if (mPinnedModelList.indexOf(item) != TabModel.INVALID_TAB_INDEX) return;

        int insertionIndex = mPinnedModelList.size();
        if (mTabModel != null) {
            int tabId = item.model.get(TabProperties.TAB_ID);
            Map<Integer, Integer> tabIdToModelIndex = new HashMap<>();
            for (int i = 0; i < mTabModel.getCount(); i++) {
                Tab tab = mTabModel.getTabAt(i);
                if (tab != null) {
                    tabIdToModelIndex.put(tab.getId(), i);
                }
            }
            int indexInModel = tabIdToModelIndex.getOrDefault(tabId, TabModel.INVALID_TAB_INDEX);
            if (indexInModel != TabModel.INVALID_TAB_INDEX) {
                insertionIndex = 0;
                while (insertionIndex < mPinnedModelList.size()) {
                    ListItem currentItem = mPinnedModelList.get(insertionIndex);
                    int currentTabId = currentItem.model.get(TabProperties.TAB_ID);
                    int currentIndexInModel =
                            tabIdToModelIndex.getOrDefault(
                                    currentTabId, TabModel.INVALID_TAB_INDEX);
                    if (currentIndexInModel != TabModel.INVALID_TAB_INDEX
                            && currentIndexInModel > indexInModel) {
                        break;
                    }
                    insertionIndex++;
                }
            }
        }
        mPinnedModelList.add(insertionIndex, item);
        notifyVisibilityChangedIfNeeded();
    }

    private void removeTabFromPinnedModelList(int tabId) {
        int index = mPinnedModelList.indexFromTabId(tabId);
        if (index != TabModel.INVALID_TAB_INDEX) {
            mPinnedModelList.removeAt(index);
            notifyVisibilityChangedIfNeeded();
        }
    }

    private void notifyVisibilityChangedIfNeeded() {
        boolean isEmpty = mPinnedModelList.isEmpty();
        if (isEmpty != mWasEmpty) {
            mWasEmpty = isEmpty;
            mOnVisibilityChanged.run();
        }
    }
}
