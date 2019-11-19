// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.MotionEventSynthesizer;

/**
 * Forwards events for Java native UI pages to MotionEventSynthesizer.
 */
@JNINamespace("vr")
public class AndroidUiGestureTarget {
    private final MotionEventSynthesizer mMotionEventSynthesizer;
    private final long mNativePointer;
    private long mLastTimestampMs;

    public AndroidUiGestureTarget(
            View target, float scaleFactor, float scrollRatio, int touchSlop) {
        mMotionEventSynthesizer = MotionEventSynthesizer.create(target);
        mNativePointer = AndroidUiGestureTargetJni.get().init(
                AndroidUiGestureTarget.this, scaleFactor, scrollRatio, touchSlop);
    }

    @CalledByNative
    private void inject(int action, long timeInMs) {
        mMotionEventSynthesizer.inject(action, 1 /* pointerCount */, timeInMs);
        mLastTimestampMs = timeInMs;
    }

    @CalledByNative
    private void setPointer(int x, int y) {
        mMotionEventSynthesizer.setPointer(
                0 /* index */, x, y, 0 /* id */, MotionEvent.TOOL_TYPE_STYLUS);
    }

    @CalledByNative
    private void setDelayedEvent(int x, int y, int action, long timeInMs, int delayMs) {
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                long delayedTimestampMs = timeInMs + delayMs;
                // Drop this event if it's been delayed too long and another event has already
                // been injected.
                if (mLastTimestampMs > delayedTimestampMs) return;
                mMotionEventSynthesizer.setPointer(
                        0 /* index */, x, y, 0 /* id */, MotionEvent.TOOL_TYPE_STYLUS);
                mMotionEventSynthesizer.inject(action, 1 /* pointerCount */, delayedTimestampMs);
                mLastTimestampMs = delayedTimestampMs;
            }
        }, delayMs);
    }

    @CalledByNative
    private long getNativeObject() {
        return mNativePointer;
    }

    @NativeMethods
    interface Natives {
        long init(
                AndroidUiGestureTarget caller, float scaleFactor, float scrollRatio, int touchSlop);
    }
}
