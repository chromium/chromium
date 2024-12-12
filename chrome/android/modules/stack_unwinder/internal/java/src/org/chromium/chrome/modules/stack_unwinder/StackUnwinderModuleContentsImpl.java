// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.jni_zero.NativeMethods;

/**
 * Provides access to the stack unwinder native code functions within the dynamic feature module.
 */
public class StackUnwinderModuleContentsImpl implements StackUnwinderModuleContents {

    @Override
    public long getDoNothingFunction() {
        return StackUnwinderModuleContentsImplJni.get().getDoNothingFunction();
    }

    @NativeMethods("stack_unwinder")
    interface Natives {
        long getDoNothingFunction();
    }
}
