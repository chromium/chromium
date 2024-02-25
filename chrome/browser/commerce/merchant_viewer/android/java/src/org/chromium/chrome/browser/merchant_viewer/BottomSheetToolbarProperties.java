// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

/** BottomSheetToolbar UI properties. */
public class BottomSheetToolbarProperties {
    public static final WritableObjectPropertyKey<GURL> URL = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableFloatPropertyKey LOAD_PROGRESS = new WritableFloatPropertyKey();

    public static final WritableBooleanPropertyKey PROGRESS_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableIntPropertyKey SECURITY_ICON = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> SECURITY_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Runnable> SECURITY_ICON_ON_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Runnable> CLOSE_BUTTON_ON_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey FAVICON_ICON = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<Drawable> FAVICON_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey FAVICON_ICON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey OPEN_IN_NEW_TAB_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                URL,
                TITLE,
                LOAD_PROGRESS,
                PROGRESS_VISIBLE,
                SECURITY_ICON,
                SECURITY_ICON_CONTENT_DESCRIPTION,
                SECURITY_ICON_ON_CLICK_CALLBACK,
                CLOSE_BUTTON_ON_CLICK_CALLBACK,
                FAVICON_ICON,
                FAVICON_ICON_DRAWABLE,
                FAVICON_ICON_VISIBLE,
                OPEN_IN_NEW_TAB_VISIBLE
            };
}
