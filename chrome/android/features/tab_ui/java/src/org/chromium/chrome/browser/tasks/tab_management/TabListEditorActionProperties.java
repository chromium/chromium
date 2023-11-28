// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties of the {@link TabListEditorAction}. */
public class TabListEditorActionProperties {
    public static final ReadableIntPropertyKey MENU_ITEM_ID = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey SHOW_MODE = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey BUTTON_TYPE = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey ICON_POSITION = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey TEXT_APPEARANCE_ID = new ReadableIntPropertyKey();

    public static final WritableIntPropertyKey TITLE_RESOURCE_ID = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey TITLE_IS_PLURAL =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Integer> CONTENT_DESCRIPTION_RESOURCE_ID =
            new WritableObjectPropertyKey();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey();
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>(true);
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey ITEM_COUNT = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<ColorStateList> TEXT_TINT =
            new WritableObjectPropertyKey<>();

    /** Tint for the icon. */
    public static final WritableObjectPropertyKey<ColorStateList> ICON_TINT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOULD_DISMISS_MENU =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Callback<List<Integer>>>
            ON_SELECTION_STATE_CHANGE = new WritableObjectPropertyKey<>();

    /** Keys for the {@link TabListEditorAction}. */
    public static final PropertyKey[] ACTION_KEYS = {
        MENU_ITEM_ID,
        SHOW_MODE,
        BUTTON_TYPE,
        ICON_POSITION,
        TEXT_APPEARANCE_ID,
        TITLE_RESOURCE_ID,
        TITLE_IS_PLURAL,
        CONTENT_DESCRIPTION_RESOURCE_ID,
        ICON,
        ENABLED,
        ITEM_COUNT,
        TEXT_TINT,
        ICON_TINT,
        ON_CLICK_LISTENER,
        SHOULD_DISMISS_MENU,
        ON_SELECTION_STATE_CHANGE
    };

    /** Keys for the {@link TabListEditorMenuItem}. */
    public static final PropertyKey[] MENU_ITEM_KEYS = {
        MENU_ITEM_ID,
        TEXT_APPEARANCE_ID,
        TITLE,
        CONTENT_DESCRIPTION,
        ICON,
        ICON_TINT,
        ENABLED,
        ITEM_COUNT
    };
}
