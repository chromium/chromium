// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * A coordinator for the menu on tab group cards in GTS. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabListGroupMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private final Activity mActivity;
    private boolean mShouldShowIcons;

    /**
     * @param onItemClicked A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param activity The {@link Context} that the coordinator resides in.
     */
    public TabListGroupMenuCoordinator(
            OnItemClickedCallback<Token> onItemClicked,
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @NonNull CollaborationService collaborationService,
            @NonNull Activity activity) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                onItemClicked,
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
    }

    /**
     * Creates a {@link TabListMediator.TabActionListener} that creates the menu and shows it when
     * clicked.
     */
    TabListMediator.TabActionListener getTabActionListener() {
        return (view, tabId) -> {
            @Nullable TabModel tabModel = getTabModel();
            if (tabModel == null) return;

            @Nullable Tab tab = tabModel.getTabById(tabId);
            if (tab == null) return;

            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) return;

            mShouldShowIcons = false;
            createAndShowMenu(view, tabGroupId, (Activity) view.getContext());
        };
    }

    /**
     * Show the context menu of the tab group with visible icons.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabGroupId The tab group ID of the interacting tab group.
     */
    public void showMenuWithIcons(RectProvider anchorViewRectProvider, Token tabGroupId) {
        mShouldShowIcons = true;
        createAndShowMenu(
                anchorViewRectProvider,
                tabGroupId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ ResourcesCompat.ID_NULL,
                AnchoredPopupWindow.HorizontalOrientation.LAYOUT_DIRECTION,
                mActivity);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Token tabGroupId) {
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        @Nullable String collaborationId = getCollaborationIdOrNull(tabGroupId);
        boolean hasCollaborationData =
                TabShareUtils.isCollaborationIdValid(collaborationId)
                        && mCollaborationService.getServiceStatus().isAllowedToJoin();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.close_tab_group_menu_item,
                        R.id.close_tab_group,
                        mShouldShowIcons ? R.drawable.ic_tab_close_24dp : Resources.ID_NULL,
                        /* iconTintColorStateList= */ Resources.ID_NULL,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.rename_tab_group_menu_item,
                        R.id.edit_group_name,
                        mShouldShowIcons ? R.drawable.ic_edit_24dp : Resources.ID_NULL,
                        /* iconTintColorStateList= */ Resources.ID_NULL,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        if (!hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.ungroup_tab_group_menu_item,
                            R.id.ungroup_tab,
                            mShouldShowIcons ? R.drawable.ic_ungroup_tabs_24dp : Resources.ID_NULL,
                            /* iconTintColorStateList= */ Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            true));
            if (!isIncognito && mCollaborationService.getServiceStatus().isAllowedToCreate()) {
                itemList.add(
                        BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                                R.string.share_tab_group_menu_item,
                                R.id.share_group,
                                mShouldShowIcons ? R.drawable.ic_group_24dp : Resources.ID_NULL,
                                /* iconTintColorStateList= */ Resources.ID_NULL,
                                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                                isIncognito,
                                true));
            }
        }
        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (mTabGroupSyncService != null && !isIncognito && !hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_tab_group,
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
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
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
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
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            /* isIncognito= */ false,
                            /* enabled= */ true));
        }
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(
                mShouldShowIcons
                        ? R.dimen.tab_group_menu_with_icons_width
                        : R.dimen.tab_group_menu_width);
    }
}
