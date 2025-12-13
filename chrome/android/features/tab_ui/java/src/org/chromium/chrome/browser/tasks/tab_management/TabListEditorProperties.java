// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link PropertyKey} list for TabListEditor. */
@NullMarked
public class TabListEditorProperties {
    public static final PropertyModel.WritableIntPropertyKey CREATION_MODE =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            DONE_BUTTON_CLICK_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey DONE_BUTTON_VISIBILITY =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_DONE_BUTTON_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList>
            TOOLBAR_BUTTON_TINT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            TOOLBAR_NAVIGATION_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> TOOLBAR_TEXT_TINT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CREATION_MODE,
                DONE_BUTTON_CLICK_HANDLER,
                DONE_BUTTON_VISIBILITY,
                IS_DONE_BUTTON_ENABLED,
                IS_VISIBLE,
                PRIMARY_COLOR,
                TOOLBAR_BACKGROUND_COLOR,
                TOOLBAR_BUTTON_TINT,
                TOOLBAR_NAVIGATION_LISTENER,
                TOOLBAR_TEXT_TINT,
                TOOLBAR_TITLE,
                TOP_MARGIN
            };
}
