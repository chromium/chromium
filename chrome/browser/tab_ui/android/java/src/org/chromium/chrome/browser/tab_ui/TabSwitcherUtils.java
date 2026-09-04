// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

/** Utility methods for TabSwitcher related actions. */
@NullMarked
public class TabSwitcherUtils {
    /**
     * Returns whether the Grid Tab Switcher should be disabled. True when the
     * DisableGridTabSwitcher feature flag is enabled on Desktop Android.
     *
     * <p>In test environments, this defaults to false unless explicitly overridden via
     * {@code @EnableFeatures} or {@link FeatureOverrides}.
     *
     * TODO(crbug.com/545634112): This may also be considered for tablets behind a different flag.
     */
    public static boolean isGridTabSwitcherDisabled() {
        if (!DeviceInfo.isDesktop()) {
            return false;
        }
        if (BuildConfig.IS_FOR_TEST) {
            Boolean testValue =
                    FeatureOverrides.getTestValueForFeature(
                            ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER);
            if (testValue == null) {
                return false;
            }
            return testValue;
        }
        return ChromeFeatureList.sDisableGridTabSwitcher.isEnabled();
    }

    /**
     * A method to navigate to tab switcher.
     *
     * @param layoutManager A {@link LayoutManagerChrome} used to watch for scene changes.
     * @param animate Whether the transition should be animated if the layout supports it.
     * @param onNavigationFinished Runnable to run after navigation to TabSwitcher is finished.
     */
    public static void navigateToTabSwitcher(
            LayoutManager layoutManager, boolean animate, @Nullable Runnable onNavigationFinished) {
        if (isGridTabSwitcherDisabled() || layoutManager.isLayoutVisible(LayoutType.HUB)) {
            if (onNavigationFinished != null) {
                onNavigationFinished.run();
            }
            return;
        }

        layoutManager.addObserver(
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.HUB) {
                            layoutManager.removeObserver(this);
                            if (onNavigationFinished != null) {
                                onNavigationFinished.run();
                            }
                        }
                    }
                });

        layoutManager.showLayout(LayoutType.HUB, animate);
    }

    /**
     * Brings focus to a tab group identified by its sync ID.
     *
     * <p>If the tab group is currently closed locally, it will first be opened via the {@link
     * TabGroupUiActionHandler}.
     *
     * <p>When the Grid Tab Switcher / Hub is disabled (e.g., on Desktop Android), this selects the
     * last shown tab of the group in the {@link TabModel} to focus the group on the tab strip.
     * Otherwise, it invokes {@code requestOpenTabGroupDialog} to present the tab group dialog
     * inside the tab switcher.
     *
     * @param syncId The sync ID of the tab group, which may or may not correspond to an open group.
     * @param tabGroupSyncService Service used to retrieve sync group metadata and convert sync IDs
     *     to local IDs.
     * @param tabGroupUiActionHandler Handler used to open closed tab groups.
     * @param tabModel The tab model used to resolve the group's last shown tab and update tab
     *     selection when the tab switcher is disabled.
     * @param requestOpenTabGroupDialog Callback invoked with the root tab ID to display the tab
     *     group dialog when the tab switcher is enabled.
     */
    public static void focusTabGroup(
            String syncId,
            TabGroupSyncService tabGroupSyncService,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            TabModel tabModel,
            Callback<Integer> requestOpenTabGroupDialog) {
        SavedTabGroup syncGroup = tabGroupSyncService.getGroup(syncId);
        if (syncGroup == null) return;

        if (syncGroup.localId == null) {
            tabGroupUiActionHandler.openTabGroup(assertNonNull(syncGroup.syncId));
            syncGroup = tabGroupSyncService.getGroup(syncId);
            assert syncGroup != null;
            assert syncGroup.localId != null;
        }

        int tabId = tabModel.getGroupLastShownTabId(syncGroup.localId.tabGroupId);
        if (tabId == Tab.INVALID_TAB_ID) return;
        if (isGridTabSwitcherDisabled()) {
            Tab tab = tabModel.getTabById(tabId);
            if (tab != null) {
                tabModel.setIndex(tabModel.indexOf(tab), TabSelectionType.FROM_USER);
            }
            return;
        }
        requestOpenTabGroupDialog.onResult(tabId);
    }

    /**
     * Helper method to hide the tab switcher if it is showing, and brings focus to the given tab.
     * If another tab was showing, it switches to the given tab.
     *
     * @param tabId The ID of the tab that it should switch to.
     */
    public static void hideTabSwitcherAndShowTab(
            int tabId,
            @Nullable TabModelSelector tabModelSelector,
            @Nullable LayoutManager layoutManager) {
        if (tabModelSelector == null) return;

        TabModel tabModel = tabModelSelector.getModel(/* incognito= */ false);
        Tab tab = tabModel.getTabById(tabId);
        // If the backend sends us a non-existent tab ID, we should safely ignore.
        if (tab == null) return;

        tabModelSelector.selectModel(/* incognito= */ false);
        tabModel.setIndex(tabModel.indexOf(tab), TabSelectionType.FROM_USER);

        // If the tab-switcher is displayed, hide it to show the tab.
        if (layoutManager != null && layoutManager.isLayoutVisible(LayoutType.HUB)) {
            layoutManager.showLayout(LayoutType.BROWSING, /* animate= */ false);
        }
    }
}
