// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Class containing static methods for Clang profiling. */
@JNINamespace("base")
public class ClangProfiler {
    private ClangProfiler() {}

    /**
     * Writes Clang profiling profile to the configured path (LLVM_PROFILE_FILE).
     * No-op if use_clang_coverage = false when building.
     */
    public static void writeClangProfilingProfile() {
        ClangProfilerJni.get().writeClangProfilingProfile();
    }

    @NativeMethods
    interface Natives {
        void writeClangProfilingProfile();
    }
}
