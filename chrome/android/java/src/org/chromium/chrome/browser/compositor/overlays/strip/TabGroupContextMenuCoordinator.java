// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.DimenRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupVisualDataTextInputLayout;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListSectionDividerProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
public class TabGroupContextMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private TabGroupVisualDataTextInputLayout mTabGroupTextInputLayout;
    private ColorPickerCoordinator mColorPickerCoordinator;
    private TabGroupModelFilter mTabGroupModelFilter;
    private int mGroupRootId;
    private Context mContext;

    /**
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param actionConfirmationManager Used to show a confirmation dialog.
     * @param tabCreator The {@link TabCreator} to use to create new tab.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled.
     */
    public TabGroupContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            ActionConfirmationManager actionConfirmationManager,
            TabCreator tabCreator,
            boolean isTabGroupSyncEnabled) {
        super(
                R.layout.tab_strip_group_menu_layout,
                getMenuItemClickedCallback(
                        tabGroupModelFilter, actionConfirmationManager, tabCreator),
                tabModelSupplier,
                isTabGroupSyncEnabled,
                /* identityManager= */ null,
                /* tabGroupSyncService= */ null,
                /* dataSharingService= */ null);
        mTabGroupModelFilter = tabGroupModelFilter;
    }

    @VisibleForTesting
    static OnItemClickedCallback getMenuItemClickedCallback(
            TabGroupModelFilter tabGroupModelFilter,
            ActionConfirmationManager actionConfirmationManager,
            TabCreator tabCreator) {
        return (menuId, tabId) -> {
            if (menuId == org.chromium.chrome.R.id.ungroup_tab) {
                TabUiUtils.ungroupTabGroup(tabGroupModelFilter, actionConfirmationManager, tabId);
            } else if (menuId == org.chromium.chrome.R.id.close_tab) {
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabId,
                        /* hideTabGroups= */ true,
                        /* didCloseCallback= */ null);
            } else if (menuId == org.chromium.chrome.R.id.delete_tab) {
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        actionConfirmationManager,
                        tabId,
                        /* hideTabGroups= */ false,
                        /* didCloseCallback= */ null);
            } else if (menuId == org.chromium.chrome.R.id.open_new_tab_in_group) {
                TabUiUtils.openNtpInGroup(
                        tabGroupModelFilter, tabCreator, tabId, TabLaunchType.FROM_CHROME_UI);
            }
        };
    }

    // TODO(crbug.com/357878838): Pass the activity through constructor and make it a class
    // variable and try to test the real `createAndShowMenu` method and remove the `IS_FOR_TEST`
    // check.
    /**
     * Show the context menu of the tab group.
     *
     * @param anchorView The anchor {@link View} of the context menu.
     * @param rootId The root id of the interacting tab group.
     * @param activity The current activity.
     */
    protected void showMenu(View anchorView, int rootId, @NonNull Activity activity) {
        if (!BuildConfig.IS_FOR_TEST) {
            mGroupRootId = rootId;
            createAndShowMenu(anchorView, rootId, activity);
        }
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        mContext = contentView.getContext();

        // TODO(crbug.com/354255648): Implement afterTextChangedListener to validate input and
        // update tab group title.
        mTabGroupTextInputLayout = contentView.findViewById(R.id.tab_group_title);
        TextInputEditText titleInputText = contentView.findViewById(R.id.title_input_text);

        // TODO(crbug.com/355483736): Confirm the final horizontal padding for the menu and update
        // if necessary.
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);

        // InputEditText has automatic 4dp horizontal padding; subtract this when adding paddings to
        // TextInputLayout to align hint text and stroke with the rest of the views.
        mTabGroupTextInputLayout.setPadding(
                horizontalPadding - titleInputText.getPaddingLeft(),
                0,
                horizontalPadding - titleInputText.getPaddingRight(),
                0);

        // TODO(crbug.com/357104424): Consider create ColorPickerCoordinator once during the first
        // call, and reuse it for subsequent calls.
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mContext,
                        ColorPickerUtils.getTabGroupColorIdList(),
                        ((ViewStub) contentView.findViewById(R.id.color_picker_stub)).inflate(),
                        ColorPickerType.TAB_GROUP,
                        isIncognito,
                        ColorPickerLayoutType.DYNAMIC,
                        this::updateTabGroupColor);
        mColorPickerCoordinator
                .getContainerView()
                .setPadding(horizontalPadding, 0, horizontalPadding, 0);

        // The color picker should select the current color of the tab group when it is displayed.
        @TabGroupColorId
        int curGroupColor = mTabGroupModelFilter.getTabGroupColorWithFallback(mGroupRootId);
        mColorPickerCoordinator.setSelectedColorItem(curGroupColor);
    }

    @Override
    protected void buildMenuActionItems(
            ModelList itemList,
            boolean isIncognito,
            boolean shouldShowDeleteGroup,
            boolean hasCollaborationData) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS)
                        .with(
                                ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding)
                        .with(
                                ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID,
                                R.dimen.list_menu_item_horizontal_padding);
        itemList.add(new ListItem(ListMenuItemType.DIVIDER, builder.build()));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.open_new_tab_in_group_context_menu_item,
                        R.id.open_new_tab_in_group,
                        R.drawable.ic_open_new_tab_in_group_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.ungroup_tab_group_menu_item,
                        R.id.ungroup_tab,
                        R.drawable.ic_ungroup_tabs_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));
        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_close_group,
                        R.id.close_tab,
                        R.drawable.ic_tab_close_24dp,
                        R.color.default_icon_color_light_tint_list,
                        R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                        isIncognito,
                        true));

        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if (shouldShowDeleteGroup && !isIncognito && !hasCollaborationData) {
            itemList.add(new ListItem(ListMenuItemType.DIVIDER, builder.build()));
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.tab_grid_dialog_toolbar_delete_group,
                            R.id.delete_tab,
                            R.drawable.material_ic_delete_24dp,
                            R.color.default_icon_color_light_tint_list,
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
        // Intentional no-op.
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.tab_strip_group_context_menu_max_width;
    }

    private void updateTabGroupColor() {
        @TabGroupColorId int newColor = mColorPickerCoordinator.getSelectedColorSupplier().get();
        TabUiUtils.updateTabGroupColor(mTabGroupModelFilter, mGroupRootId, newColor);
    }

    protected TabGroupVisualDataTextInputLayout getTabGroupTextInputLayoutForTesting() {
        return mTabGroupTextInputLayout;
    }

    protected ColorPickerCoordinator getColorPickerCoordinatorForTesting() {
        return mColorPickerCoordinator;
    }
}
