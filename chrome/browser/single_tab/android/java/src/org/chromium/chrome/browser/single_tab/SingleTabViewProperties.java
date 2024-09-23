// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the single tab view properties. */
class SingleTabViewProperties {
    private SingleTabViewProperties() {}

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable>
            SEE_MORE_LINK_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> FAVICON =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> TAB_THUMBNAIL =
            new PropertyModel.WritableObjectPropertyKey<>(/* skipEquality= */ true);
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey LATERAL_MARGIN =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLICK_LISTENER,
                SEE_MORE_LINK_CLICK_LISTENER,
                FAVICON,
                TAB_THUMBNAIL,
                IS_VISIBLE,
                TITLE,
                URL,
                LATERAL_MARGIN
            };
}
