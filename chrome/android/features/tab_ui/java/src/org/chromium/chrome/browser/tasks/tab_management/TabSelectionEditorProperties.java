// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link PropertyKey} list for TabSelectionEditor.
 */
public class TabSelectionEditorProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_ACTION_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> TOOLBAR_ACTION_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel
            .WritableIntPropertyKey TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_NAVIGATION_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<ColorStateList> TOOLBAR_GROUP_BUTTON_TINT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey TOOLBAR_TEXT_APPEARANCE =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<Rect> SELECTION_EDITOR_POSITION_RECT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<
            ViewTreeObserver.OnGlobalLayoutListener> SELECTION_EDITOR_GLOBAL_LAYOUT_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_VISIBLE,
            TOOLBAR_ACTION_BUTTON_LISTENER, TOOLBAR_ACTION_BUTTON_TEXT,
            TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, TOOLBAR_NAVIGATION_LISTENER, PRIMARY_COLOR,
            TOOLBAR_BACKGROUND_COLOR, TOOLBAR_GROUP_BUTTON_TINT, TOOLBAR_TEXT_APPEARANCE,
            SELECTION_EDITOR_POSITION_RECT, SELECTION_EDITOR_GLOBAL_LAYOUT_LISTENER};
}
