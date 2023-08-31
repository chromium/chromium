// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Responsible for hosting properties of the improved bookmark row. */
public class ImprovedBookmarkRowProperties {
    @IntDef({ImageVisibility.DRAWABLE, ImageVisibility.FOLDER_DRAWABLE, ImageVisibility.MENU})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ImageVisibility {
        // Single drawable displayed.
        int DRAWABLE = 0;
        // Multiple images displayed for folders.
        int FOLDER_DRAWABLE = 1;
        // Menu displayed.
        int MENU = 2;
    }

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey DESCRIPTION_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey START_IMAGE_VISIBILITY =
            new WritableIntPropertyKey();
    // Sets the background color for the start image.
    public static final WritableIntPropertyKey START_AREA_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    // Sets the tint color for the start image.
    public static final WritableObjectPropertyKey<ColorStateList> START_ICON_TINT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> START_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View> ACCESSORY_VIEW =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ListMenuButtonDelegate>
            LIST_MENU_BUTTON_DELEGATE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> POPUP_LISTENER =
            new WritableObjectPropertyKey<>();
    // Whether the row is currently selected.
    // This means that the model won't necessarily always be up-to-date. Using skipEquality to
    // push events to the view even if the property is the same.
    public static final WritableObjectPropertyKey<Boolean> SELECTED =
            new WritableObjectPropertyKey<>(/*skipEquality=*/true);
    // Not if the row is currently selected, but whether another row in the same list is selected.
    public static final WritableBooleanPropertyKey SELECTION_ACTIVE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey DRAG_ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey EDITABLE = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<View.OnClickListener> ROW_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnLongClickListener>
            ROW_LONG_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey END_IMAGE_VISIBILITY = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey END_IMAGE_RES = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<ShoppingAccessoryCoordinator>
            SHOPPING_ACCESSORY_COORDINATOR = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ImprovedBookmarkFolderViewCoordinator>
            FOLDER_COORDINATOR = new WritableObjectPropertyKey<>();

    private static final PropertyKey[] IMPROVED_BOOKMARK_ROW_PROPERTIES = {TITLE, DESCRIPTION,
            DESCRIPTION_VISIBLE, START_IMAGE_VISIBILITY, START_AREA_BACKGROUND_COLOR,
            START_ICON_TINT, START_ICON_DRAWABLE, ACCESSORY_VIEW, LIST_MENU_BUTTON_DELEGATE,
            POPUP_LISTENER, SELECTED, SELECTION_ACTIVE, DRAG_ENABLED, EDITABLE, ROW_CLICK_LISTENER,
            ROW_LONG_CLICK_LISTENER, SHOPPING_ACCESSORY_COORDINATOR, FOLDER_COORDINATOR,
            END_IMAGE_VISIBILITY, END_IMAGE_RES};
    public static final PropertyKey[] ALL_KEYS = PropertyModel.concatKeys(
            BookmarkManagerProperties.ALL_KEYS, IMPROVED_BOOKMARK_ROW_PROPERTIES);
}
