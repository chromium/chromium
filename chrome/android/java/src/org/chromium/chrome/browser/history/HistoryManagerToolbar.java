// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;

import java.util.List;

/** The SelectionToolbar for the browsing history UI. */
public class HistoryManagerToolbar extends SelectableListToolbar<HistoryItem> {
    private HistoryManager mManager;
    private HistoryManagerMenuDelegate mMenuDelegate;

    /**
     * Interface to the Chrome preference storage used to keep the last visibility state of the info
     * header.
     */
    public interface InfoHeaderPref {
        default boolean isVisible() {
            return false;
        }

        default void setVisible(boolean visible) {}
    }

    /** Delegate for menu capabilities of history management. */
    public interface HistoryManagerMenuDelegate {
        /** Return whether deleting history is currently supported. */
        boolean supportsDeletingHistory();

        /** Return whether incognito is currently supported. */
        boolean supportsIncognito();
    }

    public HistoryManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * @param manager The {@link HistoryManager} associated with this toolbar.
     */
    public void setManager(HistoryManager manager) {
        mManager = manager;

        if (!mManager.isDisplayedInSeparateActivity()) {
            getMenu().removeItem(R.id.close_menu_id);
        }
    }

    /**
     * @param menuDelegate The {@link HistoryManagerMenuDelegate} that determines the availability
     *     of various menu items.
     */
    public void setMenuDelegate(HistoryManagerMenuDelegate menuDelegate) {
        mMenuDelegate = menuDelegate;
        updateMenuItemVisibility();
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();
        updateInfoMenuItem(
                mManager.shouldShowInfoButton(), mManager.shouldShowInfoHeaderIfAvailable());
    }

    @Override
    public void onSelectionStateChange(List<HistoryItem> selectedItems) {
        boolean wasSelectionEnabled = mIsSelectionEnabled;
        super.onSelectionStateChange(selectedItems);

        if (mIsSelectionEnabled) {
            int numSelected = mSelectionDelegate.getSelectedItems().size();

            // If the delete menu item is shown in the overflow menu instead of as an action, there
            // may not be a view associated with it.
            View deleteButton = findViewById(R.id.selection_mode_delete_menu_id);
            if (deleteButton != null) {
                deleteButton.setContentDescription(
                        getResources()
                                .getQuantityString(
                                        R.plurals.accessibility_remove_selected_items,
                                        numSelected,
                                        numSelected));
            }

            // The copy link option should only be visible when one item is selected.
            getItemById(R.id.selection_mode_copy_link).setVisible(numSelected == 1);

            if (!wasSelectionEnabled) {
                mManager.recordSelectionEstablished();
            }
        }
    }

    @Override
    public void setSearchEnabled(boolean searchEnabled) {
        super.setSearchEnabled(searchEnabled);
        updateInfoMenuItem(
                mManager.shouldShowInfoButton(), mManager.shouldShowInfoHeaderIfAvailable());
        // shouldShowInfoButton is checked to ensure all the menu items are ready.
        if (searchEnabled && mManager.shouldShowInfoButton()) {
            mManager.showIPH();
        }
    }

    /** Should be called when the user's sign in state changes. */
    public void onSignInStateChange() {
        updateMenuItemVisibility();
        updateInfoMenuItem(
                mManager.shouldShowInfoButton(), mManager.shouldShowInfoHeaderIfAvailable());
    }

    @Override
    protected void onNavigationBack() {
        mManager.finish();
    }

    private void updateMenuItemVisibility() {
        // Once the selection mode delete or incognito menu options are removed, they will not
        // be added back until the user refreshes the history UI. This could happen if the user is
        // signed in to an account that cannot remove browsing history or has incognito disabled and
        // signs out.
        assert mMenuDelegate != null;
        if (!mMenuDelegate.supportsDeletingHistory()) {
            getMenu().removeItem(R.id.selection_mode_delete_menu_id);
        }
        if (!mMenuDelegate.supportsIncognito()) {
            getMenu().removeItem(R.id.selection_mode_open_in_incognito);
        }
    }

    @VisibleForTesting
    MenuItem getItemById(int menuItemId) {
        Menu menu = getMenu();
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == menuItemId) return item;
        }
        return null;
    }

    Menu getMenuForTests() {
        return getMenu();
    }
}
