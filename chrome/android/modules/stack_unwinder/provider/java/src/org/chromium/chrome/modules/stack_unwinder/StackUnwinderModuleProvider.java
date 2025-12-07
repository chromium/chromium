// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.jni_zero.CalledByNative;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;

/** Installs and loads the stack unwinder module. */
@NullMarked
public class StackUnwinderModuleProvider {
    /** Returns true if the module is installed. */
    @CalledByNative
    public static boolean isModuleInstalled() {
        // Return false for APK builds since they do not include native library partitions.
        return BundleUtils.isIsolatedSplitInstalled(StackUnwinderModule.SPLIT_NAME);
    }

    /**
     * Installs the module asynchronously.
     *
     * Can only be called if the module is not installed.
     */
    @CalledByNative
    public static void installModule() {
        StackUnwinderModule.installDeferred();
    }

    /**
     * Ensure the module's native contents are loaded and JNI is set up. Must be invoked after the
     * module is installed and before using the functions below.
     */
    @CalledByNative
    public static void ensureNativeLoaded() {
        StackUnwinderModule.ensureNativeLoaded();
    }

    @CalledByNative
    public static long getDoNothingFunction() {
        return StackUnwinderModule.getImpl().getDoNothingFunction();
    }
}
