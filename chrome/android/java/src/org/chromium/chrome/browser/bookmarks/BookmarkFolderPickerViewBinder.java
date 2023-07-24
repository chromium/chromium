// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;
import android.widget.Button;

import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the folder picker activity. */
public class BookmarkFolderPickerViewBinder {
    public static void bindFolderRow(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkFolderPickerRowProperties.ROW_COORDINATOR) {
            model.get(BookmarkFolderPickerRowProperties.ROW_COORDINATOR)
                    .setView((ImprovedBookmarkFolderSelectRow) view);
        }
    }

    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkFolderPickerProperties.TOOLBAR_TITLE) {
            Toolbar toolbar = view.findViewById(R.id.toolbar);
            toolbar.setTitle(model.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        } else if (key == BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER) {
            Button cancelButton = view.findViewById(R.id.cancel_button);
            cancelButton.setOnClickListener(
                    model.get(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER));
        } else if (key == BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER) {
            Button moveButton = view.findViewById(R.id.move_button);
            moveButton.setOnClickListener(
                    model.get(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER));
        } else if (key == BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED) {
            Button moveButton = view.findViewById(R.id.move_button);
            moveButton.setEnabled(model.get(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED));
        }
    }
}
