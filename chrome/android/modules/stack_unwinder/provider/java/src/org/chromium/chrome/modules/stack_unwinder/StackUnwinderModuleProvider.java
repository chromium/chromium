// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.jni_zero.CalledByNative;

/** Installs and loads the stack unwinder module. */
public class StackUnwinderModuleProvider {
    /** Returns true if the module is installed. */
    @CalledByNative
    public static boolean isModuleInstalled() {
        return StackUnwinderModule.isInstalled();
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

    /**
     * Returns the pointer to the CreateMemoryRegionsMap native function within the module, encoded
     * as a long. Can be called only if the module is installed.
     */
    @CalledByNative
    public static long getCreateMemoryRegionsMapFunction() {
        return StackUnwinderModule.getImpl().getCreateMemoryRegionsMapFunction();
    }

    /**
     * Returns the pointer to the CreateNativeUnwinder native function within the module, encoded as
     * a long. Can be called only if the module is installed.
     */
    @CalledByNative
    public static long getCreateNativeUnwinderFunction() {
        return StackUnwinderModule.getImpl().getCreateNativeUnwinderFunction();
    }

    /**
     * Returns the pointer to the CreateLibunwindstackUnwinder native function within the module,
     * encoded as a long. Can be called only if the module is installed.
     */
    @CalledByNative
    public static long getCreateLibunwindstackUnwinderFunction() {
        return StackUnwinderModule.getImpl().getCreateLibunwindstackUnwinderFunction();
    }
}
