// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;

import java.util.List;

/** The SelectionToolbar for the browsing history UI. */
@NullMarked
public class HistoryManagerToolbar extends SelectableListToolbar<HistoryItem> {
    private HistoryToolbarDelegate mDelegate;

    /** Delegate for host interactions needed by the history toolbar. */
    public interface HistoryToolbarDelegate {
        /** Returns whether history is displayed in a separate activity. */
        boolean isDisplayedInSeparateActivity();

        /** Returns whether the info button should be shown. */
        boolean shouldShowInfoButton();

        /** Returns whether the info header should be shown if available. */
        boolean shouldShowInfoHeaderIfAvailable();

        /** Records that selection mode was established. */
        void recordSelectionEstablished();

        /** Shows the app-specific history IPH if eligible. */
        void showIph();

        /** Returns the list content view for focus traversal. */
        View getListContentView();

        /** Finishes the host history UI. */
        void finish();

        /** Returns whether deleting history is currently supported. */
        boolean supportsDeletingHistory();

        /** Returns whether incognito is currently supported. */
        boolean supportsIncognito();
    }

    public HistoryManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * @param delegate The {@link HistoryToolbarDelegate} associated with this toolbar.
     */
    @Initializer
    public void setDelegate(HistoryToolbarDelegate delegate) {
        mDelegate = delegate;

        if (!mDelegate.isDisplayedInSeparateActivity()) {
            getMenu().removeItem(R.id.close_menu_id);
        }
        updateMenuItemVisibility();
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();
        updateInfoMenuItem(
                mDelegate.shouldShowInfoButton(), mDelegate.shouldShowInfoHeaderIfAvailable());
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
            assumeNonNull(getItemById(R.id.selection_mode_copy_link)).setVisible(numSelected == 1);

            if (!wasSelectionEnabled) {
                mDelegate.recordSelectionEstablished();
            }
        }
    }

    @Override
    public void setSearchEnabled(boolean searchEnabled) {
        super.setSearchEnabled(searchEnabled);
        updateInfoMenuItem(
                mDelegate.shouldShowInfoButton(), mDelegate.shouldShowInfoHeaderIfAvailable());
        // shouldShowInfoButton is checked to ensure all the menu items are ready.
        if (searchEnabled && mDelegate.shouldShowInfoButton()) {
            mDelegate.showIph();
        }
    }

    @Override
    protected boolean handleEnterKeyPress() {
        return getMenu().performIdentifierAction(R.id.search_menu_id, 0);
    }

    // Move focus to list content view.
    @Override
    protected View getNextFocusForward() {
        return mDelegate.getListContentView();
    }

    /** Should be called when the user's sign in state changes. */
    public void onSignInStateChange() {
        updateMenuItemVisibility();
        updateInfoMenuItem(
                mDelegate.shouldShowInfoButton(), mDelegate.shouldShowInfoHeaderIfAvailable());
    }

    @Override
    protected void onNavigationBack() {
        mDelegate.finish();
    }

    private void updateMenuItemVisibility() {
        // Once the selection mode delete or incognito menu options are removed, they will not
        // be added back until the user refreshes the history UI. This could happen if the user is
        // signed in to an account that cannot remove browsing history or has incognito disabled and
        // signs out.
        if (!mDelegate.supportsDeletingHistory()) {
            getMenu().removeItem(R.id.selection_mode_delete_menu_id);
        }
        if (!mDelegate.supportsIncognito()) {
            getMenu().removeItem(R.id.selection_mode_open_in_incognito);
        }
    }

    @VisibleForTesting
    @Nullable MenuItem getItemById(int menuItemId) {
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
