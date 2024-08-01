// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.system.Os;

import androidx.test.InstrumentationRegistry;
import androidx.test.internal.runner.listener.InstrumentationRunListener;

import org.junit.runner.Description;
import org.junit.runner.Result;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.BuildConfig;

public class NativeCoverageInstrumentationRunListener extends InstrumentationRunListener {
    private static final String EXTRA_CLANG_COVERAGE_DEVICE_FILE =
            "BaseChromiumAndroidJUnitRunner.ClangCoverageDeviceFile";

    private static final String TAG = "NativeCovListener";

    @Override
    public void testRunStarted(Description description) throws Exception {
        setClangCoverageEnvIfEnabled();
    }

    @Override
    public void testRunFinished(Result result) throws Exception {
        writeClangCoverageProfileIfEnabled();
    }

    /** Configure the required environment variable if Clang coverage argument exists. */
    public void setClangCoverageEnvIfEnabled() {
        String clangProfileFile =
                InstrumentationRegistry.getArguments().getString(EXTRA_CLANG_COVERAGE_DEVICE_FILE);
        if (clangProfileFile != null) {
            try {
                Os.setenv("LLVM_PROFILE_FILE", clangProfileFile, /* override= */ true);
            } catch (Exception e) {
                Log.w(TAG, "failed to set LLVM_PROFILE_FILE", e);
            }
        }
    }

    /**
     * Invoke __llvm_profile_dump() to write raw clang coverage profile to device. Noop if the
     * required build flag is not set.
     */
    public void writeClangCoverageProfileIfEnabled() {
        if (BuildConfig.WRITE_CLANG_PROFILING_DATA && LibraryLoader.getInstance().isInitialized()) {
            ClangProfiler.writeClangProfilingProfile();
        }
    }
}
