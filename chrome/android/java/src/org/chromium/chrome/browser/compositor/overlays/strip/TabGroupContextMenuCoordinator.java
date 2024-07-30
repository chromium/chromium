// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupVisualDataTextInputLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
public class TabGroupContextMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private TabGroupVisualDataTextInputLayout mTabGroupTextInputLayout;
    private static final String TAG = "TabGroupContextMenu";

    /**
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param shouldShowDeleteGroup Whether to show the delete group option.
     */
    public TabGroupContextMenuCoordinator(
            OnItemClickedCallback onItemClicked,
            Supplier<TabModel> tabModelSupplier,
            boolean shouldShowDeleteGroup) {
        super(
                R.layout.tab_strip_group_menu_layout,
                onItemClicked,
                tabModelSupplier,
                shouldShowDeleteGroup,
                /* identityManager= */ null,
                /* tabGroupSyncService= */ null,
                /* dataSharingService= */ null);
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        // TODO(crbug.com/354255648): Implement afterTextChangedListener to validate input and
        // update tab group title.
        mTabGroupTextInputLayout = contentView.findViewById(R.id.tab_group_title);

        Context context = contentView.getContext();

        // TODO(crbug.com/355483736): Confirm the final horizontal padding for the menu and update
        // if necessary.
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);
        mTabGroupTextInputLayout.setPadding(horizontalPadding, 0, horizontalPadding, 0);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            ColorPickerCoordinator colorPickerCoordinator =
                    new ColorPickerCoordinator(
                            context,
                            ColorPickerUtils.getTabGroupColorIdList(),
                            ((ViewStub) contentView.findViewById(R.id.color_picker_stub)).inflate(),
                            ColorPickerType.TAB_GROUP,
                            isIncognito,
                            ColorPickerLayoutType.DYNAMIC,
                            // TODO(crbug.com/354257045): Implement onColorItemClickedListener to
                            // update tab group color.
                            () -> {
                                Log.i(TAG, "Color icon clicked.");
                            });
            colorPickerCoordinator
                    .getContainerView()
                    .setPadding(horizontalPadding, 0, horizontalPadding, 0);
        }
    }

    @Override
    protected void buildMenuActionItems(
            ModelList modelList,
            boolean isIncognito,
            boolean shouldShowDeleteGroup,
            boolean hasCollaborationData) {
        // TODO(crbug.com/354248683): Implement icon and texts for items like ungroup, close group
        // and open new tab in group.
    }

    @Override
    public void buildCollaborationMenuItems(
            ModelList itemList,
            IdentityManager identityManager,
            GroupDataOrFailureOutcome outcome) {
        // Intentional no-op.
    }

    @Override
    protected int getMenuWidth() {
        return 0;
    }
}
