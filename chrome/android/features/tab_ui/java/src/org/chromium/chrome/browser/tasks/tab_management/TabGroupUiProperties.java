// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link PropertyKey} list for the TabGroupUi. */
class TabGroupUiProperties {
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            LEFT_BUTTON_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            RIGHT_BUTTON_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey LEFT_BUTTON_DRAWABLE_ID =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    public static final PropertyModel.WritableObjectPropertyKey<String>
            LEFT_BUTTON_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String>
            RIGHT_BUTTON_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                LEFT_BUTTON_ON_CLICK_LISTENER,
                RIGHT_BUTTON_ON_CLICK_LISTENER,
                IS_MAIN_CONTENT_VISIBLE,
                IS_INCOGNITO,
                LEFT_BUTTON_DRAWABLE_ID,
                INITIAL_SCROLL_INDEX,
                LEFT_BUTTON_CONTENT_DESCRIPTION,
                RIGHT_BUTTON_CONTENT_DESCRIPTION
            };
}
