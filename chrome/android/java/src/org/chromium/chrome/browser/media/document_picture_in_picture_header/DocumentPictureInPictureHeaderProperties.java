// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;

import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/**
 * Properties for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class defines the keys for the property model used by the header.
 */
@NullMarked
public class DocumentPictureInPictureHeaderProperties {
    public static final WritableBooleanPropertyKey IS_SHOWN = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey HEADER_HEIGHT = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Insets> HEADER_SPACING =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<ColorStateList> TINT_COLOR_LIST =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            ON_BACK_TO_TAB_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnLayoutChangeListener>
            ON_LAYOUT_CHANGE_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<List<Rect>> NON_DRAGGABLE_AREAS =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    public static final WritableBooleanPropertyKey IS_BACK_TO_TAB_SHOWN =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey SECURITY_ICON = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            ON_SECURITY_ICON_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> URL_STRING =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BRANDED_COLOR_SCHEME = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        IS_SHOWN,
        HEADER_HEIGHT,
        HEADER_SPACING,
        BACKGROUND_COLOR,
        TINT_COLOR_LIST,
        ON_BACK_TO_TAB_CLICK_LISTENER,
        ON_LAYOUT_CHANGE_LISTENER,
        NON_DRAGGABLE_AREAS,
        IS_BACK_TO_TAB_SHOWN,
        SECURITY_ICON,
        SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID,
        ON_SECURITY_ICON_CLICK_LISTENER,
        URL_STRING,
        BRANDED_COLOR_SCHEME
    };
}
