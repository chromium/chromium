// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Responsible for hosting properties of the improved bookmark row. */
class ImprovedBookmarkRowProperties {
    @IntDef({StartImageVisibility.DRAWABLE, StartImageVisibility.FOLDER_DRAWABLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartImageVisibility {
        int DRAWABLE = 0;
        int FOLDER_DRAWABLE = 1;
    }

    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> DESCRIPTION = new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey START_IMAGE_VISIBILITY = new WritableIntPropertyKey();
    // Sets the background color for the start image, both for the image and folder view.
    static final WritableIntPropertyKey START_AREA_BACKGROUND_COLOR = new WritableIntPropertyKey();
    // Sets the tint color for the start image, both for the image and folder view.
    static final WritableObjectPropertyKey<ColorStateList> START_ICON_TINT =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Drawable> START_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Pair<Drawable, Drawable>> START_IMAGE_FOLDER_DRAWABLES =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey FOLDER_CHILD_COUNT = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<View> ACCESSORY_VIEW = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<ListMenuButtonDelegate> LIST_MENU_BUTTON_DELEGATE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> POPUP_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey SELECTION_ACTIVE = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey DRAG_ENABLED = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey EDITABLE = new WritableBooleanPropertyKey();
    static final WritableObjectPropertyKey<Runnable> OPEN_BOOKMARK_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<ShoppingAccessoryCoordinator>
            SHOPPING_ACCESSORY_COORDINATOR = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {BookmarkManagerProperties.BOOKMARK_LIST_ENTRY,
            BookmarkManagerProperties.BOOKMARK_ID, BookmarkManagerProperties.LOCATION, TITLE,
            DESCRIPTION, START_IMAGE_VISIBILITY, START_AREA_BACKGROUND_COLOR, START_ICON_TINT,
            START_ICON_DRAWABLE, START_IMAGE_FOLDER_DRAWABLES, FOLDER_CHILD_COUNT, ACCESSORY_VIEW,
            LIST_MENU_BUTTON_DELEGATE, POPUP_LISTENER, SELECTED, SELECTION_ACTIVE, DRAG_ENABLED,
            EDITABLE, OPEN_BOOKMARK_CALLBACK, SHOPPING_ACCESSORY_COORDINATOR};
}
