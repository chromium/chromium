// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

/** A Java API for calling the native QR Code Generator. */
public class QRCodeGenerator {
    public static @Nullable Bitmap generateBitmap(String data) {
        return QRCodeGeneratorJni.get().generateBitmap(data);
    }

    @NativeMethods
    interface Natives {
        @Nullable
        Bitmap generateBitmap(String data);
    }
}
