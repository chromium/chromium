// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for binding properties to BookmarkToolbar views. */
class BookmarkToolbarViewBinder {
    /** Binds the given property to the given model for the given view. */
    public static void bind(PropertyModel model, BookmarkToolbar bookmarkToolbar, PropertyKey key) {
        if (key == BookmarkToolbarProperties.BOOKMARK_OPENER) {
            bookmarkToolbar.setBookmarkOpener(model.get(BookmarkToolbarProperties.BOOKMARK_OPENER));
        } else if (key == BookmarkToolbarProperties.SELECTION_DELEGATE) {
            bookmarkToolbar.setSelectionDelegate(
                    model.get(BookmarkToolbarProperties.SELECTION_DELEGATE));
        } else if (key == BookmarkToolbarProperties.BOOKMARK_UI_MODE) {
            bookmarkToolbar.setBookmarkUiMode(
                    model.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE));
        } else if (key == BookmarkToolbarProperties.TITLE) {
            bookmarkToolbar.setTitle(model.get(BookmarkToolbarProperties.TITLE));
        } else if (key == BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE) {
            bookmarkToolbar.setSoftKeyboardVisible(
                    Boolean.TRUE.equals(
                            model.get(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE)));
        } else if (key == BookmarkToolbarProperties.IS_DIALOG_UI) {
            bookmarkToolbar.setIsDialogUi(model.get(BookmarkToolbarProperties.IS_DIALOG_UI));
        } else if (key == BookmarkToolbarProperties.DRAG_ENABLED) {
            bookmarkToolbar.setDragEnabled(model.get(BookmarkToolbarProperties.DRAG_ENABLED));
        } else if (key == BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE) {
            bookmarkToolbar.setEditButtonVisible(
                    model.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        } else if (key == BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE) {
            bookmarkToolbar.setNewFolderButtonVisible(
                    model.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        } else if (key == BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED) {
            bookmarkToolbar.setNewFolderButtonEnabled(
                    model.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));
        } else if (key == BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE) {
            bookmarkToolbar.setNavigationButtonState(
                    model.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE));
        } else if (key == BookmarkToolbarProperties.SORT_MENU_IDS) {
            bookmarkToolbar.setSortMenuIds(model.get(BookmarkToolbarProperties.SORT_MENU_IDS));
        } else if (key == BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED) {
            bookmarkToolbar.setSortMenuIdsEnabled(
                    model.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        } else if (key == BookmarkToolbarProperties.CHECKED_SORT_MENU_ID) {
            bookmarkToolbar.setCheckedSortMenuId(
                    model.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        } else if (key == BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID) {
            bookmarkToolbar.setCheckedViewMenuId(
                    model.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
        } else if (key == BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE) {
            bookmarkToolbar.setNavigateBackRunnable(
                    model.get(BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE));
        } else if (key == BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION) {
            bookmarkToolbar.setMenuIdClickedFunction(
                    model.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION));
        } else if (key == BookmarkToolbarProperties.FAKE_SELECTION_STATE_CHANGE) {
            bookmarkToolbar.fakeSelectionStateChange();
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT) {
            bookmarkToolbar.setSelectionShowEdit(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB) {
            bookmarkToolbar.setSelectionShowOpenInNewTab(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO) {
            bookmarkToolbar.setSelectionShowOpenInIncognito(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE) {
            bookmarkToolbar.setSelectionShowMove(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ) {
            bookmarkToolbar.setSelectionShowMarkRead(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        } else if (key == BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD) {
            bookmarkToolbar.setSelectionShowMarkUnread(
                    model.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
        }
    }
}
