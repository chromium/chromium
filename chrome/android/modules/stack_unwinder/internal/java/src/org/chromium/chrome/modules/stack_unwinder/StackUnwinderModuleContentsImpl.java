// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.UsedByReflection;

/**
 * Provides access to the stack unwinder native code functions within the dynamic feature module.
 */
@UsedByReflection("StackUnwinderModule")
@MainDex
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

    @NativeMethods
    interface Natives {
        long getCreateMemoryRegionsMapFunction();
        long getCreateNativeUnwinderFunction();
    }
}
