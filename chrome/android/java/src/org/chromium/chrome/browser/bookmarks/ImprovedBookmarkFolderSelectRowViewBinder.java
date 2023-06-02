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
public class ImprovedBookmarkFolderSelectRowViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkFolderSelectRow row = (ImprovedBookmarkFolderSelectRow) view;
        if (key == ImprovedBookmarkFolderSelectRowProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.START_AREA_BACKGROUND_COLOR) {
            row.setStartAreaBackgroundColor(model.get(
                    ImprovedBookmarkFolderSelectRowProperties.START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.START_ICON_DRAWABLE) {
            row.setStartIconDrawable(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.START_ICON_DRAWABLE));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.START_ICON_TINT) {
            row.setStartIconTint(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.START_ICON_TINT));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES) {
            Pair<Drawable, Drawable> drawables = model.get(
                    ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES);
            row.setStartImageDrawables(drawables.first, drawables.second);
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.FOLDER_CHILD_COUNT) {
            row.setChildCount(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.FOLDER_CHILD_COUNT));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE) {
            row.setEndIconVisible(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE));
        } else if (key == ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER) {
            row.setRowClickListener(
                    model.get(ImprovedBookmarkFolderSelectRowProperties.ROW_CLICK_LISTENER));
        }
    }
}
