// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class ScreenshotShareSheetViewProperties {
    /**
     * Callback to perform the specified operation. Argument to callback must be a
     * NoArgOperation
     */
    public static final WritableObjectPropertyKey<Callback<Integer>> NO_ARG_OPERATION_LISTENER =
            new WritableObjectPropertyKey<Callback<Integer>>();

    public static final WritableObjectPropertyKey<Bitmap> SCREENSHOT_BITMAP =
            new WritableObjectPropertyKey<>();

    /**
     * Set of operations that don't require additional arguments. If a callback requires an
     * argument, it should defined separately.
     */
    @IntDef({NoArgOperation.NONE, NoArgOperation.SHARE, NoArgOperation.SAVE, NoArgOperation.DELETE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NoArgOperation {
        int NONE = 0;
        int SHARE = 1;
        int SAVE = 2;
        int DELETE = 3;
    }

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {NO_ARG_OPERATION_LISTENER, SCREENSHOT_BITMAP};
}
