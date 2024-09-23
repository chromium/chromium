// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Helper to get the screenshot captured by the native side. */
@JNINamespace("gesturenav")
public class ScreenshotCaptureTestHelper {

    interface NavScreenshotCallback {
        // If non-null, the returned Bitmap is used as the screenshot.
        // TODO(https://crbug.com/337886037) Remove return Bitmap once emulator-GPU issues are
        // worked out.
        Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested);
    }

    private NavScreenshotCallback mNavScreenshotCallback;

    void setNavScreenshotCallbackForTesting(NavScreenshotCallback callback) {
        mNavScreenshotCallback = callback;
        ScreenshotCaptureTestHelperJni.get().setNavScreenshotCallbackForTesting(this);
    }

    @CalledByNative
    Bitmap onNavScreenshotAvailable(int navIndex, Bitmap bitmap, boolean requested) {
        if (mNavScreenshotCallback == null) return null;
        return mNavScreenshotCallback.onAvailable(navIndex, bitmap, requested);
    }

    @NativeMethods
    public interface Natives {
        void setNavScreenshotCallbackForTesting(ScreenshotCaptureTestHelper helper);
    }
}
