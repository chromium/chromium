// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ImprovedBookmarkRow.  */
class ImprovedBookmarkRowViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkRow row = (ImprovedBookmarkRow) view;
        if (key == ImprovedBookmarkRowProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkRowProperties.TITLE));
        } else if (key == ImprovedBookmarkRowProperties.DESCRIPTION) {
            row.setDescription(model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        } else if (key == ImprovedBookmarkRowProperties.ICON) {
            row.setIcon(model.get(ImprovedBookmarkRowProperties.ICON));
        } else if (key == ImprovedBookmarkRowProperties.ACCESSORY_VIEW) {
            row.setAccessoryView(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
        } else if (key == ImprovedBookmarkRowProperties.LIST_MENU) {
            row.setListMenu(model.get(ImprovedBookmarkRowProperties.LIST_MENU));
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
        }
    }
}
