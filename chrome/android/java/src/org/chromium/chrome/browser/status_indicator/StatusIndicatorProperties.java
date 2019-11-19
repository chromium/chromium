// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class StatusIndicatorProperties {
    /** The text that describes status. */
    static final PropertyModel.WritableObjectPropertyKey<String> STATUS_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The {@link Drawable} that will be displayed next to the status text. */
    static final PropertyModel.WritableObjectPropertyKey<Drawable> STATUS_ICON =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Whether the Android view version of the status indicator is visible. */
    static final PropertyModel.WritableBooleanPropertyKey ANDROID_VIEW_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    /** Whether the composited version of the status indicator is visible. */
    static final PropertyModel.WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            STATUS_TEXT, STATUS_ICON, ANDROID_VIEW_VISIBLE, COMPOSITED_VIEW_VISIBLE};
}
