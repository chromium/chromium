// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Handles flushing of background attributions that arrived before the native library was loaded.
 */
public class BackgroundAttributionFlusher {
    private long mNativePtr;

    @CalledByNative
    private BackgroundAttributionFlusher(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private void flushPreNativeAttributions() {
        BackgroundAttributionFlusherImpl.flushPreNativeAttributions(() -> {
            if (mNativePtr != 0) {
                BackgroundAttributionFlusherJni.get().onFlushComplete(mNativePtr);
            }
        });
    }

    @CalledByNative
    private void nativeDestroyed() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void onFlushComplete(long nativeBackgroundAttributionFlusher);
    }
}
