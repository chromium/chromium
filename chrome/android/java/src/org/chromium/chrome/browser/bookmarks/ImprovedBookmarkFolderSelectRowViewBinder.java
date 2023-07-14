// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ImprovedBookmarkFolderSelectRow.  */
public class ImprovedBookmarkFolderSelectRowViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkFolderSelectRow row = (ImprovedBookmarkFolderSelectRow) view;
        if (key == ImprovedBookmarkFolderSelectRowProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE) {
            row.setEndIconVisible(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER) {
            row.setRowClickListener(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER));
        }
    }
}
