// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class DesktopPopupHeaderProperties {
    public static final WritableBooleanPropertyKey IS_SHOWN = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<String> TITLE_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey TITLE_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey TITLE_APPEARANCE = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Insets> TITLE_SPACING =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey HEADER_HEIGHT_PX = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        IS_SHOWN,
        TITLE_TEXT,
        TITLE_VISIBLE,
        TITLE_APPEARANCE,
        TITLE_SPACING,
        BACKGROUND_COLOR,
        HEADER_HEIGHT_PX
    };
}
