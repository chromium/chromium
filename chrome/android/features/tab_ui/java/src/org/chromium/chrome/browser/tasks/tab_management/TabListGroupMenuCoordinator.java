// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.function.Supplier;

/**
 * A coordinator for the menu on tab group cards in GTS. It is responsible for creating a list of
 * menu items, setting up the menu and displaying the menu.
 */
@NullMarked
public class TabListGroupMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private final Activity mActivity;
    private final boolean mShouldShowIcons;
    private boolean mIsMenuFocusableUponCreation;

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
            CollaborationService collaborationService,
            Activity activity) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                R.layout.tab_switcher_action_menu_layout,
                onItemClicked,
                tabModelSupplier,
                /* multiInstanceManager= */ null,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
        mShouldShowIcons = ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled();
    }

    /** Creates a {@link TabActionListener} that creates the menu and shows it when clicked. */
    TabActionListener getTabActionListener() {
        return new TabActionListener() {
            @Override
            public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                @Nullable TabModel tabModel = getTabModel();
                if (tabModel == null) return;

                @Nullable Tab tab = tabModel.getTabById(tabId);
                if (tab == null) return;

                @Nullable Token tabGroupId = tab.getTabGroupId();
                if (tabGroupId == null) return;

                mIsMenuFocusableUponCreation = true;
                createAndShowMenu(
                        new ViewRectProvider(view),
                        tabGroupId,
                        /* horizontalOverlapAnchor= */ true,
                        /* verticalOverlapAnchor= */ true,
                        /* animStyle= */ Resources.ID_NULL,
                        MAX_AVAILABLE_SPACE,
                        mActivity,
                        /* isIncognito= */ tabModel.isIncognitoBranded());
            }

            @Override
            public void run(View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                // Intentional no-op.
            }
        };
    }

    /**
     * Show the context menu of the tab group.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabGroupId The tab group ID of the interacting tab group.
     * @param focusable True if the menu should be focusable by default, false otherwise.
     */
    public void showMenu(RectProvider anchorViewRectProvider, Token tabGroupId, boolean focusable) {
        mIsMenuFocusableUponCreation = focusable;
        createAndShowMenu(
                anchorViewRectProvider,
                tabGroupId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                MAX_AVAILABLE_SPACE,
                mActivity,
                mTabModelSupplier.get().isIncognitoBranded());
    }

    @Override
    protected void afterCreate() {
        // Update the focusable state before the menu window is shown to prevent the menu from
        // stealing focus from other components.
        setMenuFocusable(mIsMenuFocusableUponCreation);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Token tabGroupId) {
        @Nullable String collaborationId = getCollaborationIdOrNull(tabGroupId);
        boolean hasCollaborationData =
                TabShareUtils.isCollaborationIdValid(collaborationId)
                        && mCollaborationService.getServiceStatus().isAllowedToJoin();
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();

        itemList.add(
                buildListItem(
                        R.string.close_tab_group_menu_item,
                        R.id.close_tab_group,
                        mShouldShowIcons ? R.drawable.ic_tab_close_24dp : Resources.ID_NULL,
                        isIncognito));
        itemList.add(
                buildListItem(
                        R.string.rename_tab_group_menu_item,
                        R.id.edit_group_name,
                        mShouldShowIcons ? R.drawable.ic_edit_24dp : Resources.ID_NULL,
                        isIncognito));

        if (!hasCollaborationData) {
            itemList.add(
                    buildListItem(
                            R.string.ungroup_tab_group_menu_item,
                            R.id.ungroup_tab,
                            mShouldShowIcons ? R.drawable.ic_ungroup_tabs_24dp : Resources.ID_NULL,
                            isIncognito));
            if (!isIncognito && mCollaborationService.getServiceStatus().isAllowedToCreate()) {
                itemList.add(buildShareMenuItem(R.string.share_tab_group_menu_item));
            }
        } else {
            assert !isIncognito;
            itemList.add(buildShareMenuItem(R.string.tab_grid_manage_button_text));
        }

        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (mTabGroupSyncService != null && !isIncognito && !hasCollaborationData) {

            itemList.add(
                    buildListItem(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_tab_group,
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
                            /* isIncognito= */ false));
        }
    }

    @Override
    public void buildCollaborationMenuItems(ModelList itemList, @MemberRole int memberRole) {
        if (memberRole == MemberRole.OWNER) {

            itemList.add(
                    buildListItem(
                            R.string.delete_tab_group_menu_item,
                            R.id.delete_shared_group,
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
                            /* isIncognito= */ false));
        } else if (memberRole == MemberRole.MEMBER) {
            itemList.add(
                    buildListItem(
                            R.string.leave_tab_group_menu_item,
                            R.id.leave_group,
                            mShouldShowIcons
                                    ? R.drawable.material_ic_delete_24dp
                                    : Resources.ID_NULL,
                            /* isIncognito= */ false));
        }
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(
                mShouldShowIcons
                        ? R.dimen.tab_group_menu_with_icons_width
                        : R.dimen.tab_group_menu_width);
    }

    private ListItem buildShareMenuItem(@StringRes int stringId) {
        return new ListItemBuilder()
                .withTitleRes(stringId)
                .withMenuId(R.id.share_group)
                .withStartIconRes(mShouldShowIcons ? R.drawable.ic_group_24dp : Resources.ID_NULL)
                .withTextAppearanceStyle(R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                .build();
    }

    private static ListItem buildListItem(
            @StringRes int titleRes,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean isIncognito) {
        return new ListItemBuilder()
                .withTitleRes(titleRes)
                .withMenuId(menuId)
                .withStartIconRes(startIconId)
                .withIsIncognito(isIncognito)
                .withTextAppearanceStyle(R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                .build();
    }
}
