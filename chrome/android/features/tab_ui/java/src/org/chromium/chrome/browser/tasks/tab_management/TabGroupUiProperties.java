// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** {@link PropertyKey} list for the TabGroupUi. */
@NullMarked
class TabGroupUiProperties {
    public static final WritableObjectPropertyKey<OnClickListener>
            SHOW_GROUP_DIALOG_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener>
            NEW_TAB_BUTTON_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey SHOW_GROUP_DIALOG_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IMAGE_TILES_CONTAINER_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<ColorStateList> TINT =
            new WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link WritableIntPropertyKey} so that we can force update on the same
     * value.
     */
    public static final WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new WritableObjectPropertyKey<>(true);

    public static final WritableObjectPropertyKey<Callback<Integer>> WIDTH_PX_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHOW_GROUP_DIALOG_ON_CLICK_LISTENER,
                NEW_TAB_BUTTON_ON_CLICK_LISTENER,
                IS_MAIN_CONTENT_VISIBLE,
                BACKGROUND_COLOR,
                SHOW_GROUP_DIALOG_BUTTON_VISIBLE,
                IMAGE_TILES_CONTAINER_VISIBLE,
                TINT,
                INITIAL_SCROLL_INDEX,
                WIDTH_PX_CALLBACK,
            };
}
