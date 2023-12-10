// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.toolbar;

import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;

import java.util.List;

/** Handles toolbar functionality for the download home. */
public class DownloadHomeToolbar extends SelectableListToolbar<ListItem> {
    private UiConfig mUiConfig;

    public DownloadHomeToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.download_manager_menu);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        post(
                () -> {
                    mUiConfig = new UiConfig(this);
                    configureWideDisplayStyle(mUiConfig);
                });
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mUiConfig != null) mUiConfig.updateDisplayStyle();
    }

    /**
     * Removes a menu item from the toolbar.
     * @param menuItemId The menu item to be removed. Nothing happens if there is no menu item
     *                   associated with this ID.
     */
    public void removeMenuItem(int menuItemId) {
        getMenu().removeItem(menuItemId);
    }

    @Override
    public void onSelectionStateChange(List<ListItem> selectedItems) {
        boolean wasSelectionEnabled = mIsSelectionEnabled;
        super.onSelectionStateChange(selectedItems);

        if (mIsSelectionEnabled) {
            int numSelected = mSelectionDelegate.getSelectedItems().size();

            // If the share or delete menu items are shown in the overflow menu instead of as an
            // action, there may not be views associated with them.
            View shareButton = findViewById(R.id.selection_mode_share_menu_id);
            if (shareButton != null) {
                // Sharing functionality that leads directly to the Android share sheet is
                // currently disabled.
                if (BuildInfo.getInstance().isAutomotive) {
                    shareButton.setVisibility(View.GONE);
                } else {
                    shareButton.setContentDescription(
                            getResources()
                                    .getQuantityString(
                                            R.plurals.accessibility_share_selected_items,
                                            numSelected,
                                            numSelected));
                }
            }

            View deleteButton = findViewById(R.id.selection_mode_delete_menu_id);
            if (deleteButton != null) {
                deleteButton.setContentDescription(
                        getResources()
                                .getQuantityString(
                                        R.plurals.accessibility_remove_selected_items,
                                        numSelected,
                                        numSelected));
            }

            if (!wasSelectionEnabled) {
                RecordUserAction.record("Android.DownloadManager.SelectionEstablished");
            }
        }
    }
}
