// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.EditText;

import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Toolbar for controlling the list of history clusters in the Journeys UI.
 */
class HistoryClustersToolbar extends SelectableListToolbar<PropertyModel> {
    private EditText mSearchText;

    /**
     * Constructor for inflating from XML.
     */
    public HistoryClustersToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.history_clusters_menu);
    }

    @Override
    public void initializeSearchView(
            SearchDelegate searchDelegate, int hintStringResId, int searchMenuItemId) {
        super.initializeSearchView(searchDelegate, hintStringResId, searchMenuItemId);
        mSearchText = findViewById(R.id.search_text);
    }

    @Override
    public void onSelectionStateChange(List selectedItems) {
        super.onSelectionStateChange(selectedItems);
        if (mIsSelectionEnabled) {
            getMenu()
                    .findItem(R.id.selection_mode_copy_link)
                    .setVisible(mSelectionDelegate.getSelectedItems().size() == 1);
        }
    }

    boolean isSearchTextFocused() {
        return mSearchText.isFocused();
    }

    void setSearchText(String text, boolean wantFocus) {
        if (!text.equals(mSearchText.getText().toString())) {
            mSearchText.setText(text);
            mSearchText.setSelection(text.length());
        }

        if (wantFocus) return;
        mSearchText.clearFocus();
    }
}
