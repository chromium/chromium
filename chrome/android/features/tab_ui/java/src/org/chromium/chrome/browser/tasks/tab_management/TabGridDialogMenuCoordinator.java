// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
 * A coordinator for the menu in TabGridDialog toolbar. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
public class TabGridDialogMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    /**
     * Creates a {@link View.OnClickListener} that creates the menu and shows it when clicked.
     *
     * @param onItemClickedCallback The clicked listener callback that handles clicks on menu items.
     * @param currentTabIdSupplier The supplier of the current tab ID.
     * @param isIncognitoSupplier Whether the current tab group model filter is in an incognito
     *     state.
     * @param shouldShowDeleteGroup Whether to show the delete group option.
     * @param tabModelSupplier Used for fetching tab data.
     * @param identityManager Used for checking the current account.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param dataSharingService Used for checking the user is the owner of a group.
     * @return A {@link View.OnClickListener} for the button that opens up the menu.
     */
    static View.OnClickListener getTabGridDialogMenuOnClickListener(
            OnItemClickedCallback onItemClickedCallback,
            Supplier<Integer> currentTabIdSupplier,
            Supplier<Boolean> isIncognitoSupplier,
            boolean shouldShowDeleteGroup,
            @Nullable Supplier<TabModel> tabModelSupplier,
            @Nullable IdentityManager identityManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService) {
        return view -> {
            Context context = view.getContext();
            TabGridDialogMenuCoordinator menu =
                    new TabGridDialogMenuCoordinator(
                            context,
                            view,
                            onItemClickedCallback,
                            currentTabIdSupplier.get(),
                            isIncognitoSupplier.get(),
                            shouldShowDeleteGroup,
                            tabModelSupplier,
                            identityManager,
                            tabGroupSyncService,
                            dataSharingService);
            menu.display();
        };
    }

    private class OnGroupDataOrFailureOutcome implements Callback<GroupDataOrFailureOutcome> {
        private final ModelList mItemList;
        private final boolean mIsIncognito;

        OnGroupDataOrFailureOutcome(ModelList itemList, boolean isIncognito) {
            mItemList = itemList;
            mIsIncognito = isIncognito;
        }

        @Override
        public void onResult(GroupDataOrFailureOutcome outcome) {
            @MemberRole int memberRole = TabShareUtils.getSelfMemberRole(outcome, mIdentityManager);
            if (memberRole == MemberRole.OWNER) {
                mItemList.add(
                        BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                                R.string.leave_tab_group_menu_item,
                                R.id.delete_shared_group,
                                R.drawable.material_ic_delete_24dp,
                                R.color.default_icon_color_light_tint_list,
                                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                                mIsIncognito,
                                /* enabled= */ true));
            } else if (memberRole == MemberRole.MEMBER) {
                mItemList.add(
                        BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                                R.string.leave_tab_group_menu_item,
                                R.id.leave_group,
                                R.drawable.material_ic_delete_24dp,
                                R.color.default_icon_color_light_tint_list,
                                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                                mIsIncognito,
                                /* enabled= */ true));
            }
        }
    }

    private final @Nullable Supplier<TabModel> mTabModelSupplier;
    private final @Nullable IdentityManager mIdentityManager;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable DataSharingService mDataSharingService;

    @VisibleForTesting
    public TabGridDialogMenuCoordinator(
            Context context,
            View anchorView,
            OnItemClickedCallback onItemClicked,
            int tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup,
            @Nullable Supplier<TabModel> tabModelSupplier,
            @Nullable IdentityManager identityManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService) {
        super(
                context,
                R.layout.tab_switcher_action_menu_layout,
                anchorView,
                onItemClicked,
                tabId,
                isIncognito,
                shouldShowDeleteGroup);
        mTabModelSupplier = tabModelSupplier;
        mIdentityManager = identityManager;
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
    }

    @Override
    protected ModelList buildMenuActionItems(boolean isIncognito, boolean shouldShowDeleteGroup) {
        ModelList itemList = new ModelList();
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.menu_select_tabs,
                        R.id.select_tabs,
                        R.drawable.ic_select_check_box_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        /* enabled= */ true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_edit_group_name,
                        R.id.edit_group_name,
                        R.drawable.material_ic_edit_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        /* enabled= */ true));
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.tab_grid_dialog_toolbar_edit_group_color,
                            R.id.edit_group_color,
                            R.drawable.ic_colorize_24dp,
                            R.color.default_icon_color_light_tint_list,
                            R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                            isIncognito,
                            /* enabled= */ true));
        }
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_close_group,
                        R.id.close_tab,
                        R.drawable.ic_tab_close_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        /* enabled= */ true));
        if (shouldShowDeleteGroup && !isIncognito) {
            @Nullable String collaborationId = getCollaborationIdOrNull();
            if (TextUtils.isEmpty(collaborationId)
                    || mIdentityManager == null
                    || mDataSharingService == null) {
                itemList.add(
                        BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                                R.string.tab_grid_dialog_toolbar_delete_group,
                                R.id.delete_tab,
                                R.drawable.material_ic_delete_24dp,
                                R.color.default_icon_color_light_tint_list,
                                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                                isIncognito,
                                /* enabled= */ true));
            } else {
                mDataSharingService.readGroup(
                        collaborationId, new OnGroupDataOrFailureOutcome(itemList, isIncognito));
            }
        }
        return itemList;
    }

    private @Nullable String getCollaborationIdOrNull() {
        if (mTabModelSupplier == null) {
            return null;
        } else {
            return TabShareUtils.getCollaborationIdOrNull(
                    mTabId, mTabModelSupplier.get(), mTabGroupSyncService);
        }
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.menu_width;
    }
}
