// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupVisualDataTextInputLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
public class TabGroupContextMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private TabGroupVisualDataTextInputLayout mTabGroupTextInputLayout;
    private Context mContext;
    private boolean mIsIncognito;
    private static final String TAG = "TabGroupContextMenu";

    public TabGroupContextMenuCoordinator(
            Context context,
            View anchorView,
            OnItemClickedCallback onItemClicked,
            int tabId,
            boolean isIncognito,
            boolean shouldShowDeleteGroup) {
        super(
                context,
                R.layout.tab_strip_group_menu_layout,
                anchorView,
                onItemClicked,
                tabId,
                isIncognito,
                shouldShowDeleteGroup);
        mContext = context;
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        // TODO(crbug.com/354255648): Implement afterTextChangedListener to validate input and
        // update tab group title.
        mTabGroupTextInputLayout = contentView.findViewById(R.id.tab_group_title);

        // TODO(crbug.com/355483736): Confirm the final horizontal padding for the menu and update
        // if necessary.
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);
        mTabGroupTextInputLayout.setPadding(horizontalPadding, 0, horizontalPadding, 0);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            ColorPickerCoordinator colorPickerCoordinator =
                    new ColorPickerCoordinator(
                            mContext,
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
    protected ModelList buildMenuActionItems(boolean isIncognito, boolean shouldShowDeleteGroup) {
        // TODO(crbug.com/354248683): Implement icon and texts for items like ungroup, close group
        // and open new tab in group.
        return null;
    }

    @Override
    protected int getMenuWidth() {
        return 0;
    }
}
