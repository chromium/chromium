// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** Java API which exposes the registered histograms on the native side as JSON test. */
@JNINamespace("base::android")
public final class StatisticsRecorderAndroid {
    private StatisticsRecorderAndroid() {}

    /**
     * @param verbosityLevel controls the information that should be included when dumping each of
     * the histogram.
     * @return All the registered histograms as JSON text.
     */
    public static String toJson(@JSONVerbosityLevel int verbosityLevel) {
        return StatisticsRecorderAndroidJni.get().toJson(verbosityLevel);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String toJson(@JSONVerbosityLevel @JniType("JSONVerbosityLevel") int verbosityLevel);
    }
}
