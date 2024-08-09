// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

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
     * @param didCloseCallback Run after the close confirmation to indicate if a close happened.
     */
    public static void closeTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId,
            boolean hideTabGroups,
            @Nullable Callback<Boolean> didCloseCallback) {
        TabModel tabModel = filter.getTabModel();
        int rootId = tabModel.getTabById(tabId).getRootId();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        boolean isIncognito = filter.isIncognitoBranded();

        if (hideTabGroups || isIncognito) {
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
     * Update the tab group color.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param rootId The root id of the interacting tab group.
     * @param newGroupColor The new group color being assigned to the tab group.
     */
    public static void updateTabGroupColor(
            TabGroupModelFilter filter, int rootId, @TabGroupColorId int newGroupColor) {
        int curGroupColor = filter.getTabGroupColor(rootId);
        boolean didChangeColor = curGroupColor != newGroupColor;
        if (didChangeColor) {
            filter.setTabGroupColor(rootId, newGroupColor);
        }
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
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID);
        TabModel tabModel = filter.getTabModel();
        Profile profile = tabModel.getProfile();
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        @Nullable
        SavedTabGroup savedTabGroup =
                TabGroupSyncUtils.getSavedTabGroupFromTabId(tabId, tabModel, tabGroupSyncService);
        if (savedTabGroup == null || TextUtils.isEmpty(savedTabGroup.collaborationId)) return;

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
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID);
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

        actionConfirmationManager.processLeaveGroupAttempt(
                savedTabGroup.title,
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        dataSharingService.removeMember(
                                savedTabGroup.collaborationId, account.getEmail(), null);
                    }
                });
    }
}
