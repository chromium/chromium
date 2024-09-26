// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import android.os.Debug;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;

import java.io.IOException;

/** Enables the generation of hprof files from heap dumps. */
public final class JavaHeapDumpGenerator {
    private static final String TAG = "JavaHprofGenerator";

    private JavaHeapDumpGenerator() {}

    /**
     * Generates an hprof file at the given filePath.
     *
     * @return whether or not the hprof file was properly generated.
     */
    @CalledByNative
    public static boolean generateHprof(String filePath) {
        try {
            Debug.dumpHprofData(filePath);
        } catch (IOException e) {
            Log.i(TAG, "Error writing to file " + filePath + ". Error: " + e.getMessage());
            return false;
        }
        return true;
    }
}
