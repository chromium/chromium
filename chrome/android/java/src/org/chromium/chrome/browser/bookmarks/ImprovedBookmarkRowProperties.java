// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of the improved bookmark row. */
class ImprovedBookmarkRowProperties {
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> DESCRIPTION = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Drawable> ICON = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<View> ACCESSORY_VIEW = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<ListMenu> LIST_MENU = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> POPUP_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey SELECTION_ACTIVE = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey DRAG_ENABLED = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey EDITABLE = new WritableBooleanPropertyKey();
    static final WritableObjectPropertyKey<Runnable> OPEN_BOOKMARK_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {TITLE, DESCRIPTION, ICON, ACCESSORY_VIEW, LIST_MENU,
            POPUP_LISTENER, SELECTED, SELECTION_ACTIVE, DRAG_ENABLED, EDITABLE,
            OPEN_BOOKMARK_CALLBACK};
}
