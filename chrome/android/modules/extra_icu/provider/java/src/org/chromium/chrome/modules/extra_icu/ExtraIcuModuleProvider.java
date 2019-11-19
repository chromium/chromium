// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.extra_icu;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Java side of the extra ICU module installer. */
@JNINamespace("extra_icu")
public class ExtraIcuModuleProvider {
    /** Returns true if the extra ICU module is installed. */
    @CalledByNative
    private static boolean isModuleInstalled() {
        return ExtraIcuModule.isInstalled();
    }

    private ExtraIcuModuleProvider() {}
}
