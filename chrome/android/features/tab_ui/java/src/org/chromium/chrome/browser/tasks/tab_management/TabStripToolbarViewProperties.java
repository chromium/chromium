// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link PropertyKey} list for the toolbar view on tabstrip.
 */
class TabStripToolbarViewProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> EXPAND_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> ADD_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> TINT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {EXPAND_CLICK_LISTENER,
            ADD_CLICK_LISTENER, IS_MAIN_CONTENT_VISIBLE, PRIMARY_COLOR, TINT};
}
