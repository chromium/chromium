// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.jni_zero.NativeMethods;

/**
 * Provides access to the stack unwinder native code functions within the dynamic feature module.
 */
public class StackUnwinderModuleContentsImpl implements StackUnwinderModuleContents {
    /**
     * Returns the pointer to the CreateMemoryRegionsMap native function within the module, encoded
     * as a long.
     */
    @Override
    public long getCreateMemoryRegionsMapFunction() {
        return StackUnwinderModuleContentsImplJni.get().getCreateMemoryRegionsMapFunction();
    }

    /**
     * Returns the pointer to the CreateNativeUnwinder native function within the module, encoded as
     * a long.
     */
    @Override
    public long getCreateNativeUnwinderFunction() {
        return StackUnwinderModuleContentsImplJni.get().getCreateNativeUnwinderFunction();
    }

    /**
     * Returns the pointer to the CreateLibunwindstackUnwinder native function within the module,
     * encoded as a long.
     */
    @Override
    public long getCreateLibunwindstackUnwinderFunction() {
        return StackUnwinderModuleContentsImplJni.get().getCreateLibunwindstackUnwinderFunction();
    }

    @NativeMethods("stack_unwinder")
    interface Natives {
        long getCreateMemoryRegionsMapFunction();

        long getCreateNativeUnwinderFunction();

        long getCreateLibunwindstackUnwinderFunction();
    }
}
