// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.DimenRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the menu on tab group cards in GTS. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabListGroupMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    /**
     * @param onItemClicked A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     */
    public TabListGroupMenuCoordinator(
            OnItemClickedCallback onItemClicked,
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @NonNull CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                onItemClicked,
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService);
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
                        R.id.close_tab_group,
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
            if (!isIncognito && mCollaborationService.getServiceStatus().isAllowedToCreate()) {
                itemList.add(
                        BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                                R.string.share_tab_group_menu_item,
                                R.id.share_group,
                                /* startIconId= */ Resources.ID_NULL,
                                /* iconTintColorStateList= */ Resources.ID_NULL,
                                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                                isIncognito,
                                true));
            }
        }
        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (isTabGroupSyncEnabled && !isIncognito && !hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_tab_group,
                            /* startIconId= */ Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
        }
    }

    @Override
    public void buildCollaborationMenuItems(ModelList itemList, @MemberRole int memberRole) {
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
