// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for ImprovedBookmarkRow. */
public class ImprovedBookmarkRowViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        ImprovedBookmarkRow row = (ImprovedBookmarkRow) view;
        ImprovedBookmarkFolderView folderView = view.findViewById(R.id.folder_view);
        if (key == ImprovedBookmarkRowProperties.ENABLED) {
            row.setRowEnabled(model.get(ImprovedBookmarkRowProperties.ENABLED));
        } else if (key == ImprovedBookmarkRowProperties.TITLE) {
            row.setTitle(model.get(ImprovedBookmarkRowProperties.TITLE));
        } else if (key == ImprovedBookmarkRowProperties.DESCRIPTION) {
            row.setDescription(model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        } else if (key == ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE) {
            row.setDescriptionVisible(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        } else if (key == ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY) {
            int startImageVisibility =
                    model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY);
            assert startImageVisibility != ImageVisibility.MENU;
            row.setStartImageVisible(startImageVisibility == ImageVisibility.DRAWABLE);
            row.setFolderViewVisible(startImageVisibility == ImageVisibility.FOLDER_DRAWABLE);
        } else if (key == ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR) {
            row.setStartAreaBackgroundColor(
                    model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkRowProperties.START_ICON_TINT) {
            row.setStartIconTint(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        } else if (key == ImprovedBookmarkRowProperties.START_ICON_DRAWABLE) {
            row.setStartIconDrawable(null);
            model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE)
                    .onAvailable(row::setStartIconDrawable);
            model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE).get();
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
        } else if (key == ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER) {
            row.setRowClickListener(
                    (ignored) -> model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER).run());
        } else if (key == ImprovedBookmarkRowProperties.ROW_LONG_CLICK_LISTENER) {
            row.setRowLongClickListener(
                    (ignored) ->
                            model.get(ImprovedBookmarkRowProperties.ROW_LONG_CLICK_LISTENER)
                                    .getAsBoolean());
        } else if (key == ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY) {
            int endImageVisibility = model.get(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY);
            assert endImageVisibility != ImageVisibility.FOLDER_DRAWABLE;
            row.setEndImageVisible(endImageVisibility == ImageVisibility.DRAWABLE);
            row.setEndMenuVisible(endImageVisibility == ImageVisibility.MENU);
        } else if (key == ImprovedBookmarkRowProperties.END_IMAGE_RES) {
            row.setEndImageRes(model.get(ImprovedBookmarkRowProperties.END_IMAGE_RES));
        } else if (key == ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION) {
            row.setContentDescription(model.get(ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION));
        } else if (key == ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK) {
            row.setIsLocalBookmark(model.get(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK));
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR) {
            folderView.setStartAreaBackgroundColor(
                    model.get(ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR));
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT) {
            folderView.setStartIconTint(
                    model.get(ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT));
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE) {
            folderView.setStartIconDrawable(
                    model.get(ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE));
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES) {
            folderView.setStartImageDrawables(null, null);
            model.get(ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES)
                    .onAvailable(folderView::setStartImageDrawablePair);
            model.get(ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES).get();
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT) {
            folderView.setChildCount(model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));
        } else if (key == ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE) {
            folderView.setChildCountStyle(
                    model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE));
        } else if (key == BookmarkManagerProperties.IS_HIGHLIGHTED) {
            View highlightedView = view.findViewById(R.id.container);
            if (model.get(BookmarkManagerProperties.IS_HIGHLIGHTED)) {
                HighlightParams params = new HighlightParams(HighlightShape.RECTANGLE);
                params.setNumPulses(1);
                params.setCornerRadius(
                        view.getContext()
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.improved_bookmark_row_outer_corner_radius));
                ViewHighlighter.turnOnHighlight(highlightedView, params);
            } else {
                // We need this in case we are change state during a pulse.
                ViewHighlighter.turnOffHighlight(highlightedView);
            }
        }
    }
}
