// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.MenuItem;
import android.view.View;

import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the folder picker activity. */
public class BookmarkFolderPickerViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkFolderPickerProperties.TOOLBAR_TITLE) {
            Toolbar toolbar = view.findViewById(R.id.toolbar);
            toolbar.setTitle(model.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        } else if (key == BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER) {
            View cancelButton = view.findViewById(R.id.cancel_button);
            cancelButton.setOnClickListener(
                    (ignored) ->
                            model.get(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER).run());
        } else if (key == BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER) {
            View moveButton = view.findViewById(R.id.move_button);
            moveButton.setOnClickListener(
                    (ignored) ->
                            model.get(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER).run());
        } else if (key == BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED) {
            View moveButton = view.findViewById(R.id.move_button);
            moveButton.setEnabled(model.get(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED));
        } else if (key == BookmarkFolderPickerProperties.ADD_NEW_FOLDER_BUTTON_ENABLED) {
            Toolbar toolbar = view.findViewById(R.id.toolbar);
            MenuItem addNewFolderMenuItem =
                    toolbar.getMenu().findItem(R.id.create_new_folder_menu_id);
            // The containing mediator will be initialized before the menu.
            if (addNewFolderMenuItem != null) {
                addNewFolderMenuItem.setEnabled(
                        model.get(BookmarkFolderPickerProperties.ADD_NEW_FOLDER_BUTTON_ENABLED));
            }
        }
    }
}
