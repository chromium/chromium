// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ImprovedBookmarkFolderSelectRow.  */
public class ImprovedBookmarkFolderSelectViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkFolderSelectView row = (ImprovedBookmarkFolderSelectView) view;
        if (key == ImprovedBookmarkFolderSelectViewProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkFolderSelectViewProperties.TITLE));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.START_AREA_BACKGROUND_COLOR) {
            row.setStartAreaBackgroundColor(model.get(
                    ImprovedBookmarkFolderSelectViewProperties.START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.START_ICON_DRAWABLE) {
            row.setStartIconDrawable(
                    model.get(ImprovedBookmarkFolderSelectViewProperties.START_ICON_DRAWABLE));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.START_ICON_TINT) {
            row.setStartIconTint(
                    model.get(ImprovedBookmarkFolderSelectViewProperties.START_ICON_TINT));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.START_IMAGE_FOLDER_DRAWABLES) {
            Pair<Drawable, Drawable> drawables = model.get(
                    ImprovedBookmarkFolderSelectViewProperties.START_IMAGE_FOLDER_DRAWABLES);
            row.setStartImageDrawables(drawables.first, drawables.second);
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.FOLDER_CHILD_COUNT) {
            row.setChildCount(
                    model.get(ImprovedBookmarkFolderSelectViewProperties.FOLDER_CHILD_COUNT));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.END_ICON_VISIBLE) {
            row.setEndIconVisible(
                    model.get(ImprovedBookmarkFolderSelectViewProperties.END_ICON_VISIBLE));
        } else if (key == ImprovedBookmarkFolderSelectViewProperties.ROW_CLICK_LISTENER) {
            row.setRowClickListener(
                    model.get(ImprovedBookmarkFolderSelectViewProperties.ROW_CLICK_LISTENER));
        }
    }
}
