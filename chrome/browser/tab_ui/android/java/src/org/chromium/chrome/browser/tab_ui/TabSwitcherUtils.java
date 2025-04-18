// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

/** Utility methods for TabSwitcher related actions. */
@NullMarked
public class TabSwitcherUtils {
    /**
     * A method to navigate to tab switcher.
     *
     * @param layoutManager A {@link LayoutManagerChrome} used to watch for scene changes.
     * @param animate Whether the transition should be animated if the layout supports it.
     * @param onNavigationFinished Runnable to run after navigation to TabSwitcher is finished.
     */
    public static void navigateToTabSwitcher(
            LayoutManager layoutManager, boolean animate, @Nullable Runnable onNavigationFinished) {
        if (layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            if (onNavigationFinished != null) {
                onNavigationFinished.run();
            }
            return;
        }

        layoutManager.addObserver(
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            layoutManager.removeObserver(this);
                            if (onNavigationFinished != null) {
                                onNavigationFinished.run();
                            }
                        }
                    }
                });

        layoutManager.showLayout(LayoutType.TAB_SWITCHER, animate);
    }

    /**
     * Tries to open the tab group dialog for a tab group.
     *
     * @param syncId The id of the tab group, might or might not correspond to an open group.
     * @param tabGroupSyncService Used to open closed groups and convert to local ids.
     * @param tabGroupUiActionHandler Used to open a closed group.
     * @param tabGroupModelFilter Used to get root id.
     * @param requestOpenTabGroupDialog Callback to actually open a group dialog.
     */
    public static void openTabGroupDialog(
            String syncId,
            TabGroupSyncService tabGroupSyncService,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            TabGroupModelFilter tabGroupModelFilter,
            Callback<Integer> requestOpenTabGroupDialog) {
        SavedTabGroup syncGroup = tabGroupSyncService.getGroup(syncId);
        if (syncGroup == null) return;

        if (syncGroup.localId == null) {
            tabGroupUiActionHandler.openTabGroup(assertNonNull(syncGroup.syncId));
            syncGroup = tabGroupSyncService.getGroup(syncId);
            assert syncGroup != null;
            assert syncGroup.localId != null;
        }

        int rootId = tabGroupModelFilter.getRootIdFromTabGroupId(syncGroup.localId.tabGroupId);
        if (rootId == Tab.INVALID_TAB_ID) return;
        requestOpenTabGroupDialog.onResult(rootId);
    }

    /**
     * Helper method to hide the tab switcher if it is showing, and brings focus to the given tab.
     * If another tab was showing, it switches to the given tab.
     *
     * @param tabId The ID of the tab that it should switch to.
     */
    public static void hideTabSwitcherAndShowTab(
            int tabId, TabModelSelector tabModelSelector, LayoutManager layoutManager) {
        if (tabModelSelector == null) return;

        TabModel tabModel = tabModelSelector.getModel(/* incognito= */ false);
        int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabId);
        // If the backend sends us a non-existent tab ID, we should safely ignore.
        if (tabIndex == TabModel.INVALID_TAB_INDEX) return;

        tabModelSelector.selectModel(/* incognito= */ false);
        tabModel.setIndex(tabIndex, TabSelectionType.FROM_USER);

        // If the tab-switcher is displayed, hide it to show the tab.
        if (layoutManager != null && layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            layoutManager.showLayout(LayoutType.BROWSING, /* animate= */ false);
        }
    }
}
