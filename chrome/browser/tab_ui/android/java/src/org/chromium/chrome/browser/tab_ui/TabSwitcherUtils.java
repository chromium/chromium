// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

/** Utility methods for TabSwitcher related actions. */
public class TabSwitcherUtils {
    /**
     * A method to navigate to tab switcher.
     *
     * @param layoutManager A {@link LayoutManagerChrome} used to watch for scene changes.
     * @param animate Whether the transition should be animated if the layout supports it.
     * @param onNavigationFinished Runnable to run after navigation to TabSwitcher is finished.
     */
    public static void navigateToTabSwitcher(
            LayoutManager layoutManager, boolean animate, Runnable onNavigationFinished) {
        if (layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            onNavigationFinished.run();
            return;
        }

        layoutManager.addObserver(
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            layoutManager.removeObserver(this);
                            onNavigationFinished.run();
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
        if (syncGroup.localId == null) {
            tabGroupUiActionHandler.openTabGroup(syncGroup.syncId);
            syncGroup = tabGroupSyncService.getGroup(syncId);
            assert syncGroup.localId != null;
        }

        int rootId = tabGroupModelFilter.getRootIdFromStableId(syncGroup.localId.tabGroupId);
        if (rootId == Tab.INVALID_TAB_ID) return;
        requestOpenTabGroupDialog.onResult(rootId);
    }
}
