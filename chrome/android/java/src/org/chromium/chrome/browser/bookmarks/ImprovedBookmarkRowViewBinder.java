// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.StartImageVisibility;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ImprovedBookmarkRow. */
class ImprovedBookmarkRowViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkRow row = (ImprovedBookmarkRow) view;
        if (key == ImprovedBookmarkRowProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkRowProperties.TITLE));
        } else if (key == ImprovedBookmarkRowProperties.DESCRIPTION) {
            row.setDescription(model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        } else if (key == ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY) {
            int startImageVisibility =
                    model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY);
            row.setStartImageVisible(startImageVisibility == StartImageVisibility.DRAWABLE);
            row.setFolderViewVisible(startImageVisibility == StartImageVisibility.FOLDER_DRAWABLE);
        } else if (key == ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR) {
            row.setStartAreaBackgroundColor(
                    model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkRowProperties.START_ICON_TINT) {
            row.setStartIconTint(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        } else if (key == ImprovedBookmarkRowProperties.START_ICON_DRAWABLE) {
            row.setStartIconDrawable(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        } else if (key == ImprovedBookmarkRowProperties.START_IMAGE_FOLDER_DRAWABLES) {
            Pair<Drawable, Drawable> drawables =
                    model.get(ImprovedBookmarkRowProperties.START_IMAGE_FOLDER_DRAWABLES);
            row.setStartImageDrawables(drawables.first, drawables.second);
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT) {
            row.setFolderChildCount(model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));
        } else if (key == ImprovedBookmarkRowProperties.ACCESSORY_VIEW) {
            row.setAccessoryView(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
        } else if (key == ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE) {
            row.setListMenuButtonDelegate(
                    model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        } else if (key == ImprovedBookmarkRowProperties.POPUP_LISTENER) {
            row.setPopupListener(
                    () -> model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER).run());
        } else if (key == ImprovedBookmarkRowProperties.SELECTED) {
            row.setIsSelected(model.get(ImprovedBookmarkRowProperties.SELECTED));
        } else if (key == ImprovedBookmarkRowProperties.SELECTION_ACTIVE) {
            row.setSelectionEnabled(model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        } else if (key == ImprovedBookmarkRowProperties.DRAG_ENABLED) {
            row.setDragEnabled(model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        } else if (key == ImprovedBookmarkRowProperties.EDITABLE) {
            row.setBookmarkIdEditable(model.get(ImprovedBookmarkRowProperties.EDITABLE));
        } else if (key == ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK) {
            row.setOpenBookmarkCallback(
                    model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
        } else if (key == BookmarkManagerProperties.BOOKMARK_ID) {
            row.setItem(model.get(BookmarkManagerProperties.BOOKMARK_ID));
        }
    }
}
