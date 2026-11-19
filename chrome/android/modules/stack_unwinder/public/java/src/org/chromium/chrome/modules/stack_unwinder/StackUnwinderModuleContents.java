// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * Interface for the stack_unwinder dynamic feature module. The module contains no native code; it
 * exists to deliver the unwind tables used for stack unwinding on 32-bit Android.
 */
@ModuleInterface(
        module = "stack_unwinder",
        impl = "org.chromium.chrome.modules.stack_unwinder.StackUnwinderModuleContentsImpl")
@NullMarked
public interface StackUnwinderModuleContents {}
