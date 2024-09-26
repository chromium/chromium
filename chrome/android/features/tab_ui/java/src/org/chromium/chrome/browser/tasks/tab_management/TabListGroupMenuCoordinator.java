// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the menu on tab group cards in GTS. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabListGroupMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    /**
     * @param tabModelSupplier The supplier of the tab model.
     * @param isTabGroupSyncEnabled Whether to show the delete group option.
     * @param identityManager Used for checking the current account.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param dataSharingService Used for checking the user is the owner of a group.
     */
    public TabListGroupMenuCoordinator(
            OnItemClickedCallback onItemClicked,
            Supplier<TabModel> tabModelSupplier,
            boolean isTabGroupSyncEnabled,
            @Nullable IdentityManager identityManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                onItemClicked,
                tabModelSupplier,
                isTabGroupSyncEnabled,
                identityManager,
                tabGroupSyncService,
                dataSharingService);
    }

    /**
     * Creates a {@link TabListMediator.TabActionListener} that creates the menu and shows it when
     * clicked.
     */
    TabListMediator.TabActionListener getTabActionListener() {
        return (view, tabId) -> {
            createAndShowMenu(view, tabId, (Activity) view.getContext());
        };
    }

    @Override
    protected void buildMenuActionItems(
            ModelList itemList,
            boolean isIncognito,
            boolean isTabGroupSyncEnabled,
            boolean hasCollaborationData) {
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.close_tab_group_menu_item,
                        R.id.close_tab,
                        /* startIconId= */ Resources.ID_NULL,
                        /* iconTintColorStateList= */ Resources.ID_NULL,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.rename_tab_group_menu_item,
                        R.id.edit_group_name,
                        /* startIconId= */ Resources.ID_NULL,
                        /* iconTintColorStateList= */ Resources.ID_NULL,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        if (!hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.ungroup_tab_group_menu_item,
                            R.id.ungroup_tab,
                            /* startIconId= */ Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (isTabGroupSyncEnabled && !isIncognito && !hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_tab,
                            /* startIconId= */ Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
    }

    @Override
    public void buildCollaborationMenuItems(
            ModelList itemList,
            IdentityManager identityManager,
            GroupDataOrFailureOutcome outcome) {
        @MemberRole int memberRole = TabShareUtils.getSelfMemberRole(outcome, identityManager);
        if (memberRole == MemberRole.OWNER) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_shared_group,
                            /* startIconId= */ Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            /* isIncognito= */ false,
                            /* enabled= */ true));
        } else if (memberRole == MemberRole.MEMBER) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.leave_tab_group_menu_item,
                            R.id.leave_group,
                            /* startIconId= */ Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            /* isIncognito= */ false,
                            /* enabled= */ true));
        }
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.tab_group_menu_width;
    }
}
