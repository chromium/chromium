// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.res.ColorStateList;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the reader mode bottom sheet. */
@NullMarked
public class ReaderModeBottomSheetProperties {
    // TODO(crbug.com/431234334): Support multiple different tabs in for this content view.
    // TODO(crbug.com/431234338): Support creation/descruction for dynamic content views.
    public static final WritableObjectPropertyKey<View> CONTENT_VIEW =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey SECONDARY_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PRIMARY_TEXT_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey SECONDARY_TEXT_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<ColorStateList> ICON_TINT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        CONTENT_VIEW,
        BACKGROUND_COLOR,
        SECONDARY_BACKGROUND_COLOR,
        PRIMARY_TEXT_COLOR,
        SECONDARY_TEXT_COLOR,
        ICON_TINT
    };
}
