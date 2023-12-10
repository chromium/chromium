// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class ContextMenuHeaderProperties {
    /** Invalid value for OVERRIDE_*_PIXEL resources */
    static final int INVALID_OVERRIDE = -1;

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey TITLE_MAX_LINES = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<CharSequence> URL =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            TITLE_AND_URL_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey URL_MAX_LINES = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Bitmap> IMAGE = new WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey CIRCLE_BG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    /** Size in pixel of monogram / favicon on link context menu header . */
    public static final WritableIntPropertyKey MONOGRAM_SIZE_PIXEL = new WritableIntPropertyKey();

    /**
     * Size in pixel of header image & monogram. When value is not {@link #INVALID_OVERRIDE}, this
     * number will override the image size defined in the layout.
     */
    public static final WritableIntPropertyKey OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL =
            new WritableIntPropertyKey();

    /**
     * Size in pixel of circle background behind the monogram. When value is not {@link
     * #INVALID_OVERRIDE}, this number will override the image size defined in the layout.
     */
    public static final WritableIntPropertyKey OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL =
            new WritableIntPropertyKey();

    /**
     * Size in pixel of the margin around the circle background behind the monogram. When value is
     * not {@link #INVALID_OVERRIDE}, this number will override the image size defined in the
     * layout.
     */
    public static final WritableIntPropertyKey OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE,
        TITLE_MAX_LINES,
        URL,
        TITLE_AND_URL_CLICK_LISTENER,
        URL_MAX_LINES,
        IMAGE,
        CIRCLE_BG_VISIBLE,
        MONOGRAM_SIZE_PIXEL,
        OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL,
        OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL,
        OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL
    };
}
