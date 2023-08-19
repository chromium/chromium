// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for {@link ImprovedBookmarkFolderView}. */
class ImprovedBookmarkFolderViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkFolderView folderView = (ImprovedBookmarkFolderView) view;
        if (key == ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR) {
            folderView.setStartAreaBackgroundColor(
                    model.get(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkFolderViewProperties.START_ICON_TINT) {
            folderView.setStartIconTint(
                    model.get(ImprovedBookmarkFolderViewProperties.START_ICON_TINT));
        } else if (key == ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE) {
            folderView.setStartIconDrawable(
                    model.get(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE));
        } else if (key == ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES) {
            Pair<Drawable, Drawable> drawables =
                    model.get(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES);
            folderView.setStartImageDrawables(drawables.first, drawables.second);
        } else if (key == ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT) {
            folderView.setChildCount(
                    model.get(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT));
        }
    }
}
