// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class QrCodeShareViewProperties {
    /** The action that occurs when the download button is tapped. */
    public static final WritableObjectPropertyKey<Bitmap> QRCODE_BITMAP =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> ERROR_STRING =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey HAS_STORAGE_PERMISSION =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey CAN_PROMPT_FOR_PERMISSION =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey IS_ON_FOREGROUND =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        QRCODE_BITMAP,
        ERROR_STRING,
        HAS_STORAGE_PERMISSION,
        CAN_PROMPT_FOR_PERMISSION,
        IS_ON_FOREGROUND
    };
}
