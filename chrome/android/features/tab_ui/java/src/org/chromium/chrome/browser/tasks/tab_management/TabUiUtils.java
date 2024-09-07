// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Static utilities for Tab UI. */
public class TabUiUtils {

    /**
     * Closes a tab group and maybe shows a confirmation dialog.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} to use to confirm
     *     actions.
     * @param tabId The ID of one of the tabs in the tab group.
     * @param hideTabGroups Whether to hide or delete the tab group.
     * @param isSyncEnabled Whether the Tab Group Sync flag is enabled.
     * @param didCloseCallback Run after the close confirmation to indicate if a close happened.
     */
    public static void closeTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId,
            boolean hideTabGroups,
            boolean isSyncEnabled,
            @Nullable Callback<Boolean> didCloseCallback) {
        TabModel tabModel = filter.getTabModel();
        int rootId = tabModel.getTabById(tabId).getRootId();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        boolean isIncognito = filter.isIncognitoBranded();

        if (hideTabGroups || isIncognito || !isSyncEnabled || actionConfirmationManager == null) {
            filter.closeTabs(TabClosureParams.closeTabs(tabs).hideTabGroups(hideTabGroups).build());
            Callback.runNullSafe(didCloseCallback, true);
        } else {
            List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());

            // Present a confirmation dialog to the user before closing the tab group.
            Callback<Integer> onResult =
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            boolean allowUndo = result == ConfirmationResult.IMMEDIATE_CONTINUE;
                            List<Tab> tabsToClose =
                                    tabIds.stream()
                                            .map(filter.getTabModel()::getTabById)
                                            .filter(Objects::nonNull)
                                            .filter(tab -> !tab.isClosing())
                                            .collect(Collectors.toList());
                            filter.closeTabs(
                                    TabClosureParams.closeTabs(tabsToClose)
                                            .allowUndo(allowUndo)
                                            .hideTabGroups(hideTabGroups)
                                            .build());
                            Callback.runNullSafe(didCloseCallback, true);
                        } else {
                            Callback.runNullSafe(didCloseCallback, false);
                        }
                    };
            actionConfirmationManager.processDeleteGroupAttempt(onResult);
        }
    }

    /**
     * Ungroups a tab group and maybe shows a confirmation dialog.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} to use to confirm
     *     actions.
     * @param tabId The ID of one of the tabs in the tab group.
     * @param isSyncEnabled Whether the Tab Group Sync flag is enabled.
     */
    public static void ungroupTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId,
            boolean isSyncEnabled) {
        TabModel tabModel = filter.getTabModel();
        int rootId = tabModel.getTabById(tabId).getRootId();
        boolean isIncognito = filter.getTabModel().isIncognito();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());

        if (isIncognito || !isSyncEnabled || actionConfirmationManager == null) {
            for (Tab tab : tabs) {
                filter.moveTabOutOfGroup(tab.getId());
            }
        } else {
            // Present a confirmation dialog to the user before ungrouping the tab group.
            Callback<Integer> onResult =
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            List<Tab> tabsToUngroup =
                                    tabIds.stream()
                                            .map(filter.getTabModel()::getTabById)
                                            .filter(Objects::nonNull)
                                            .filter(
                                                    tab ->
                                                            !tab.isClosing()
                                                                    && filter.isTabInTabGroup(tab))
                                            .collect(Collectors.toList());
                            for (Tab tab : tabsToUngroup) {
                                filter.moveTabOutOfGroup(tab.getId());
                            }
                        }
                    };

            actionConfirmationManager.processUngroupAttempt(onResult);
        }
    }

    /**
     * Update the tab group color.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param rootId The root id of the interacting tab group.
     * @param newGroupColor The new group color being assigned to the tab group.
     * @return Whether the tab group color is updated.
     */
    public static boolean updateTabGroupColor(
            TabGroupModelFilter filter, int rootId, @TabGroupColorId int newGroupColor) {
        int curGroupColor = filter.getTabGroupColor(rootId);
        if (curGroupColor != newGroupColor) {
            filter.setTabGroupColor(rootId, newGroupColor);
            return true;
        }
        return false;
    }

    /**
     * Update the tab group title.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param rootId The root id of the interacting tab group.
     * @param newGroupTitle The new group title being assigned to the tab group.
     * @return Whether the tab group title is updated.
     */
    public static boolean updateTabGroupTitle(
            TabGroupModelFilter filter, int rootId, String newGroupTitle) {
        assert newGroupTitle != null && !newGroupTitle.isEmpty();
        String curGroupTitle = filter.getTabGroupTitle(rootId);
        if (!newGroupTitle.equals(curGroupTitle)) {
            filter.setTabGroupTitle(rootId, newGroupTitle);
            return true;
        }
        return false;
    }

    /**
     * Opens a new tab page in the last position of the tab group and selects the new tab.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param tabCreator The {@link TabCreator} to use to create new tab.
     * @param tabId The ID of one of the tabs in the tab group.
     * @param type The launch type of the new tab.
     */
    public static void openNtpInGroup(
            TabGroupModelFilter filter, TabCreator tabCreator, int tabId, @TabLaunchType int type) {
        List<Tab> relatedTabs = filter.getRelatedTabList(tabId);
        assert relatedTabs.size() > 0;

        Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
        tabCreator.createNewTab(new LoadUrlParams(UrlConstants.NTP_URL), type, parentTabToAttach);
    }

    /**
     * Deletes a shared tab group, prompting to user to verify first.
     *
     * @param filter Used to pull dependencies from.
     * @param actionConfirmationManager Used to show a confirmation dialog.
     * @param tabId The local id of the tab being deleted.
     */
    public static void deleteSharedTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING);
        TabModel tabModel = filter.getTabModel();
        Profile profile = tabModel.getProfile();
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        @Nullable
        SavedTabGroup savedTabGroup =
                TabGroupSyncUtils.getSavedTabGroupFromTabId(tabId, tabModel, tabGroupSyncService);
        if (savedTabGroup == null || TextUtils.isEmpty(savedTabGroup.collaborationId)) return;

        assert actionConfirmationManager != null;

        actionConfirmationManager.processDeleteSharedGroupAttempt(
                savedTabGroup.title,
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        dataSharingService.deleteGroup(savedTabGroup.collaborationId, null);
                    }
                });
    }

    /**
     * Leaves a shared tab group, prompting to user to verify first.
     *
     * @param filter Used to pull dependencies from.
     * @param actionConfirmationManager Used to show a confirmation dialog.
     * @param tabId The local id of the tab being left.
     */
    public static void leaveTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING);
        TabModel tabModel = filter.getTabModel();
        Profile profile = tabModel.getProfile();
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);

        @Nullable
        SavedTabGroup savedTabGroup =
                TabGroupSyncUtils.getSavedTabGroupFromTabId(tabId, tabModel, tabGroupSyncService);
        if (savedTabGroup == null || TextUtils.isEmpty(savedTabGroup.collaborationId)) return;
        @Nullable
        CoreAccountInfo account = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (account == null) return;

        assert actionConfirmationManager != null;

        actionConfirmationManager.processLeaveGroupAttempt(
                savedTabGroup.title,
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        dataSharingService.removeMember(
                                savedTabGroup.collaborationId, account.getEmail(), null);
                    }
                });
    }

    /**
     * Attaches an {@link SharedImageTilesCoordinator} to a {@link FrameLayout}.
     *
     * @param sharedImageTilesCoordinator The {@link SharedImageTilesCoordinator} to attach.
     * @param container The {@link FrameLayout} to attach to.
     */
    public static void attachSharedImageTilesCoordinatorToFrameLayout(
            SharedImageTilesCoordinator sharedImageTilesCoordinator, FrameLayout container) {
        View imageTilesView = sharedImageTilesCoordinator.getView();
        var layoutParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER);
        container.addView(imageTilesView, layoutParams);
    }
}
