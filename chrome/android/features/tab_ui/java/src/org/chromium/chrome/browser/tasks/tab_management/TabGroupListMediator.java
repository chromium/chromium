// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.AsyncDrawable;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;

/** Populates a {@link ModelList} with an item for each tab group. */
public class TabGroupListMediator {
    // Internal state enum to track where a group lives. It can either be in the current tab
    // model/window/activity, in the current activity and closing, in another one, or hidden.
    // Hidden means only the sync side know about it. Everything is assumed to be non-incognito.
    // In other tab models is difficult to work with, since often tha tab model is not even
    // loaded into memory. For currently closing groups we need to special case the behavior to
    // properly undo or commit the pending operations.
    @IntDef({
        TabGroupState.IN_CURRENT,
        TabGroupState.IN_CURRENT_CLOSING,
        TabGroupState.IN_ANOTHER,
        TabGroupState.HIDDEN,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabGroupState {
        int IN_CURRENT = 0;
        int IN_CURRENT_CLOSING = 1;
        int IN_ANOTHER = 2;
        int HIDDEN = 3;
    }

    private static final WritableObjectPropertyKey[] FAVICON_ORDER = {
        ASYNC_FAVICON_TOP_LEFT,
        ASYNC_FAVICON_TOP_RIGHT,
        ASYNC_FAVICON_BOTTOM_LEFT,
        ASYNC_FAVICON_BOTTOM_RIGHT
    };

    private final ModelList mModelList;
    private final TabGroupModelFilter mFilter;
    private final BiConsumer<GURL, Callback<Drawable>> mFaviconResolver;
    private final TabGroupSyncService mSyncService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final CallbackController mCallbackController = new CallbackController();
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void tabClosureUndone(Tab tab) {
                    // Sync events aren't sent when a tab closure is undone since sync doesn't know
                    // anything happened until the closure is committed. Make sure the UI is up to
                    // date (with the right TabGroupState) if an undo related to a tab group
                    // happens.
                    if (mFilter.isTabInTabGroup(tab)) {
                        mPendingRefresh.post();
                    }
                }
            };

    private final TabGroupSyncService.Observer mSyncObserver =
            new Observer() {
                @Override
                public void onInitialized() {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localId, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(String syncId, @TriggerSource int source) {
                    mPendingRefresh.post();
                }
            };

    /**
     * @param modelList Side effect is adding items to this list.
     * @param filter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param syncService Used to fetch synced copy of tab groups.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     */
    public TabGroupListMediator(
            ModelList modelList,
            TabGroupModelFilter filter,
            BiConsumer<GURL, Callback<Drawable>> faviconResolver,
            TabGroupSyncService syncService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ActionConfirmationManager actionConfirmationManager) {
        mModelList = modelList;
        mFilter = filter;
        mFaviconResolver = faviconResolver;
        mSyncService = syncService;
        mPaneManager = paneManager;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mActionConfirmationManager = actionConfirmationManager;

        mFilter.addObserver(mTabModelObserver);
        mSyncService.addObserver(mSyncObserver);
        repopulateModelList();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        mFilter.removeObserver(mTabModelObserver);
        mSyncService.removeObserver(mSyncObserver);
        mCallbackController.destroy();
    }

    private @TabGroupState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return TabGroupState.HIDDEN;
        }
        Token groupId = savedTabGroup.localId.tabGroupId;
        boolean isFullyClosing = true;
        int rootId = Tab.INVALID_TAB_ID;
        TabList tabList = mFilter.getTabModel().getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (groupId.equals(tab.getTabGroupId())) {
                rootId = tab.getRootId();
                isFullyClosing &= tab.isClosing();
            }
        }
        if (rootId == Tab.INVALID_TAB_ID) return TabGroupState.IN_ANOTHER;

        // If the group is only partially closing no special case is required since we still have to
        // do all the IN_CURRENT work and returning to the tab group via the dialog will work.
        return isFullyClosing ? TabGroupState.IN_CURRENT_CLOSING : TabGroupState.IN_CURRENT;
    }

    private List<Pair<SavedTabGroup, Integer>> getSortedGroupAndStateList() {
        List<Pair<SavedTabGroup, Integer>> groupAndStateList = new ArrayList<>();
        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mSyncService.getGroup(syncGroupId);
            @TabGroupState int state = getState(savedTabGroup);
            // To simplify interactions, do not include any groups currently open in other windows.
            if (state != TabGroupState.IN_ANOTHER) {
                groupAndStateList.add(new Pair<>(savedTabGroup, state));
            }
        }
        groupAndStateList.sort(
                (a, b) -> Long.compare(b.first.creationTimeMs, a.first.creationTimeMs));
        return groupAndStateList;
    }

    private void repopulateModelList() {
        mModelList.clear();
        for (Pair<SavedTabGroup, Integer> groupAndState : getSortedGroupAndStateList()) {
            SavedTabGroup savedTabGroup = groupAndState.first;
            @TabGroupState int state = groupAndState.second;

            PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
            int numberOfTabs = savedTabGroup.savedTabs.size();
            int numberOfCorners = FAVICON_ORDER.length;
            int standardCorners = numberOfCorners - 1;
            for (int i = 0; i < standardCorners; i++) {
                if (numberOfTabs > i) {
                    builder.with(
                            FAVICON_ORDER[i], buildAsyncDrawable(savedTabGroup.savedTabs.get(i)));
                } else {
                    break;
                }
            }
            if (numberOfTabs == numberOfCorners) {
                builder.with(
                        FAVICON_ORDER[standardCorners],
                        buildAsyncDrawable(savedTabGroup.savedTabs.get(standardCorners)));
            } else if (numberOfTabs > numberOfCorners) {
                builder.with(PLUS_COUNT, numberOfTabs - standardCorners);
            }

            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                builder.with(COLOR_INDEX, savedTabGroup.color);
            }

            String userTitle = savedTabGroup.title;
            Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
            builder.with(TITLE_DATA, titleData);

            builder.with(CREATION_MILLIS, savedTabGroup.creationTimeMs);

            builder.with(
                    TabGroupRowProperties.OPEN_RUNNABLE, () -> openGroup(savedTabGroup, state));
            builder.with(
                    TabGroupRowProperties.DELETE_RUNNABLE,
                    () -> processDeleteGroup(savedTabGroup, state));

            PropertyModel propertyModel = builder.build();
            ListItem listItem = new ListItem(0, propertyModel);
            mModelList.add(listItem);
        }
    }

    private void openGroup(SavedTabGroup savedTabGroup, @TabGroupState int state) {
        state = updateStateForOpenGroup(savedTabGroup, state);
        if (state == TabGroupState.IN_CURRENT_CLOSING) {
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mFilter.getTabModel().cancelTabClosure(savedTab.localId);
                }
            }
        } else if (state == TabGroupState.HIDDEN) {
            String syncId = savedTabGroup.syncId;
            mTabGroupUiActionHandler.openTabGroup(syncId);
            savedTabGroup = mSyncService.getGroup(syncId);
            assert savedTabGroup.localId != null;
        }

        int rootId = mFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(rootId);
        assert success;
    }

    private @TabGroupState int updateStateForOpenGroup(
            SavedTabGroup savedTabGroup, @TabGroupState int previousState) {
        if (previousState != TabGroupState.IN_CURRENT_CLOSING) return previousState;

        // It is possible to "race" with the undo snackbar when IN_CURRENT_CLOSING is happening
        // since refreshing this UI is a posted task. Fall back to HIDDEN if there are no tabs
        // available to cancel the closure of.
        TabList tabList = mFilter.getTabModel().getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (tab.isClosing() && savedTabGroup.localId.tabGroupId.equals(tab.getTabGroupId())) {
                return TabGroupState.IN_CURRENT_CLOSING;
            }
        }
        return TabGroupState.HIDDEN;
    }

    private void processDeleteGroup(SavedTabGroup savedTabGroup, @TabGroupState int state) {
        mActionConfirmationManager.processDeleteGroupAttempt(
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        deleteGroup(savedTabGroup, state);
                    }
                });
    }

    private void deleteGroup(SavedTabGroup savedTabGroup, @TabGroupState int state) {
        if (state == TabGroupState.IN_CURRENT_CLOSING) {
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mFilter.getTabModel().commitTabClosure(savedTab.localId);
                }
            }
            // Because the pending closure might have been hiding or part of a closure containing
            // more tabs we need to forcibly remove the group.
            mSyncService.removeGroup(savedTabGroup.syncId);
        } else if (state == TabGroupState.IN_CURRENT) {
            int rootId = mFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
            List<Tab> tabsToClose = mFilter.getRelatedTabListForRootId(rootId);
            mFilter.closeMultipleTabs(
                    tabsToClose, /* canUndo= */ false, /* hideTabGroups= */ false);
        } else {
            assert state == TabGroupState.HIDDEN;
            mSyncService.removeGroup(savedTabGroup.syncId);
        }
    }

    private AsyncDrawable buildAsyncDrawable(SavedTabGroupTab tab) {
        return (Callback<Drawable> callback) -> mFaviconResolver.accept(tab.url, callback);
    }
}
