// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.PointerDownProvenanceRecorder.PointerDownProvenance;

/** Unit tests for {@link PointerDownProvenanceRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PointerDownProvenanceRecorderUnitTest {
    private static final String HISTOGRAM = "Android.Input.PointerDownProvenance";
    // Value of the hidden MotionEvent.FLAG_IS_ACCESSIBILITY_EVENT.
    private static final int FLAG_IS_ACCESSIBILITY_EVENT = 0x800;

    @Test
    public void testRecord() {
        assertRecords(/* deviceId= */ 3, /* flags= */ 0, PointerDownProvenance.DEVICE);
        assertRecords(/* deviceId= */ 0, /* flags= */ 0, PointerDownProvenance.DEVICE);
        assertRecords(/* deviceId= */ -1, /* flags= */ 0, PointerDownProvenance.NO_DEVICE);
        assertRecords(
                /* deviceId= */ 3,
                FLAG_IS_ACCESSIBILITY_EVENT,
                PointerDownProvenance.DEVICE_VIA_ACCESSIBILITY);
        assertRecords(
                /* deviceId= */ -1,
                FLAG_IS_ACCESSIBILITY_EVENT,
                PointerDownProvenance.NO_DEVICE_VIA_ACCESSIBILITY);
    }

    @Test
    public void testRecord_ignoresOtherActions() {
        HistogramWatcher watcher = HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM).build();

        for (int action :
                new int[] {
                    MotionEvent.ACTION_POINTER_DOWN,
                    MotionEvent.ACTION_MOVE,
                    MotionEvent.ACTION_UP,
                    MotionEvent.ACTION_CANCEL
                }) {
            PointerDownProvenanceRecorder.record(
                    motionEvent(action, /* deviceId= */ 3, /* flags= */ 0));
        }

        watcher.assertExpected();
    }

    private static void assertRecords(
            int deviceId, int flags, @PointerDownProvenance int expected) {
        HistogramWatcher watcher = HistogramWatcher.newSingleRecordWatcher(HISTOGRAM, expected);

        PointerDownProvenanceRecorder.record(motionEvent(MotionEvent.ACTION_DOWN, deviceId, flags));

        watcher.assertExpected();
    }

    private static MotionEvent motionEvent(int action, int deviceId, int flags) {
        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                action,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {new MotionEvent.PointerProperties()},
                new MotionEvent.PointerCoords[] {new MotionEvent.PointerCoords()},
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1,
                /* yPrecision= */ 1,
                deviceId,
                /* edgeFlags= */ 0,
                InputDevice.SOURCE_TOUCHSCREEN,
                flags);
    }
}
