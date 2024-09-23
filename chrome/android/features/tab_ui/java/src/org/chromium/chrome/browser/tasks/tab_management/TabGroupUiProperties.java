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
            SHOW_GROUP_DIALOG_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            NEW_TAB_BUTTON_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey SHOW_GROUP_DIALOG_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IMAGE_TILES_CONTAINER_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHOW_GROUP_DIALOG_ON_CLICK_LISTENER,
                NEW_TAB_BUTTON_ON_CLICK_LISTENER,
                IS_MAIN_CONTENT_VISIBLE,
                IS_INCOGNITO,
                BACKGROUND_COLOR,
                SHOW_GROUP_DIALOG_BUTTON_VISIBLE,
                IMAGE_TILES_CONTAINER_VISIBLE,
                INITIAL_SCROLL_INDEX,
            };
}
