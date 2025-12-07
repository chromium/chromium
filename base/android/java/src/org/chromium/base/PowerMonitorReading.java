// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Container class to pass android.os.PowerMonitorReadings from Java to Native. */
@NullMarked
@JNINamespace("base::android")
public class PowerMonitorReading {
    private final String mConsumer;
    private final long mTotalEnergy;

    public PowerMonitorReading(String consumer, long totalEnergy) {
        mConsumer = consumer;
        mTotalEnergy = totalEnergy;
    }

    @CalledByNative
    public @JniType("std::string") String getConsumer() {
        return mConsumer;
    }

    @CalledByNative
    public long getTotalEnergy() {
        return mTotalEnergy;
    }
}
