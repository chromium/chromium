// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * A utility class for mock component loader call
 */
@JNINamespace("android_webview")
public class DisableOriginTrialsSafeModeTestUtils {
    @NativeMethods
    public interface Natives {
        boolean isNonDeprecationTrialDisabled();
        boolean isDeprecationTrialDisabled();
        boolean doesPolicyExist();
        boolean isFlagSet();
    }
    // Don't instantiate this class
    private DisableOriginTrialsSafeModeTestUtils() {}
}
