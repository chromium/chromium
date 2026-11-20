// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.MotionEvent;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records Android.Input.PointerDownProvenance: whether a pointer-down has an input device and
 * whether Android flagged it as an accessibility event. The histogram summary says how to read the
 * buckets on each Android version.
 */
@NullMarked
final class PointerDownProvenanceRecorder {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(PointerDownProvenance)
    @IntDef({
        PointerDownProvenance.DEVICE,
        PointerDownProvenance.NO_DEVICE,
        PointerDownProvenance.DEVICE_VIA_ACCESSIBILITY,
        PointerDownProvenance.NO_DEVICE_VIA_ACCESSIBILITY,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PointerDownProvenance {
        // Device ID >= 0. Includes ID 0, which events built in-process without a device carry.
        int DEVICE = 0;
        // Device ID < 0: injected, e.g. by adb shell input.
        int NO_DEVICE = 1;
        // The two cases above with FLAG_IS_ACCESSIBILITY_EVENT set.
        int DEVICE_VIA_ACCESSIBILITY = 2;
        int NO_DEVICE_VIA_ACCESSIBILITY = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:PointerDownProvenance)

    // Value of the hidden MotionEvent.FLAG_IS_ACCESSIBILITY_EVENT. Android 12 and later set it on
    // events that an accessibility service made or that passed through one.
    private static final int FLAG_IS_ACCESSIBILITY_EVENT = 0x800;

    private PointerDownProvenanceRecorder() {}

    /** Records one sample if {@code event} is an ACTION_DOWN. Other events are ignored. */
    static void record(MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_DOWN) return;

        boolean hasDevice = event.getDeviceId() >= 0;
        @PointerDownProvenance int provenance;
        if ((event.getFlags() & FLAG_IS_ACCESSIBILITY_EVENT) != 0) {
            provenance =
                    hasDevice
                            ? PointerDownProvenance.DEVICE_VIA_ACCESSIBILITY
                            : PointerDownProvenance.NO_DEVICE_VIA_ACCESSIBILITY;
        } else {
            provenance = hasDevice ? PointerDownProvenance.DEVICE : PointerDownProvenance.NO_DEVICE;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Input.PointerDownProvenance",
                provenance,
                PointerDownProvenance.NUM_ENTRIES);
    }
}
