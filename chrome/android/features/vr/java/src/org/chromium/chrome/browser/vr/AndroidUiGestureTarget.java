// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Forwards events for Java native UI pages to MotionEventSynthesizer.
 */
@JNINamespace("vr")
public class AndroidUiGestureTarget {

    @CalledByNative
    private void inject(int action, long timeInMs) {
    }

    @CalledByNative
    private void setPointer(int x, int y) {
    }

    @CalledByNative
    private void setDelayedEvent(int x, int y, int action, long timeInMs, int delayMs) {
    }

    @CalledByNative
    private long getNativeObject() {
        return 0;
    }

    @NativeMethods
    interface Natives {
        long init(
                AndroidUiGestureTarget caller, float scaleFactor, float scrollRatio, int touchSlop);
    }
}
