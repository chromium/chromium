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
    // Internal tri-state enum to track where a group lives. It can either be in the current tab
    // model/window/activity, in another one, or hidden. Hidden means only the sync side know about
    // it. Everything is assumed to be non-incognito. In other tab models is difficult to work with,
    // since often tha tab model is not even loaded into memory.
    @IntDef({
        TabGroupState.IN_CURRENT,
        TabGroupState.IN_ANOTHER,
        TabGroupState.HIDDEN,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabGroupState {
        int IN_CURRENT = 0;
        int IN_ANOTHER = 1;
        int HIDDEN = 2;
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

        mSyncService.addObserver(mSyncObserver);
        repopulateModelList();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        mSyncService.removeObserver(mSyncObserver);
        mCallbackController.destroy();
    }

    private @TabGroupState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return TabGroupState.HIDDEN;
        }
        Token groupId = savedTabGroup.localId.tabGroupId;
        int rootId = mFilter.getRootIdFromStableId(groupId);
        return rootId == Tab.INVALID_TAB_ID ? TabGroupState.IN_ANOTHER : TabGroupState.IN_CURRENT;
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
        if (state == TabGroupState.HIDDEN) {
            String syncId = savedTabGroup.syncId;
            mTabGroupUiActionHandler.openTabGroup(syncId);
            savedTabGroup = mSyncService.getGroup(syncId);
            assert savedTabGroup.localId != null;
        }
        int rootId = mFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(rootId);
        assert success;
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
        if (state == TabGroupState.IN_CURRENT) {
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
