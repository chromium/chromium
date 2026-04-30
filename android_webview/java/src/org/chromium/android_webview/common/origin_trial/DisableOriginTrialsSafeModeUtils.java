// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.origin_trial;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.build.annotations.NullMarked;

/** A util class for disable origin trials Safe Mode operations. */
@JNINamespace("android_webview")
@NullMarked
public class DisableOriginTrialsSafeModeUtils {
    // Don't instantiate this class.
    private DisableOriginTrialsSafeModeUtils() {}

    @CalledByNative
    public static boolean isDisableOriginTrialsEnabled() {
        return SafeModeController.getInstance()
                .isActionEnabled(SafeModeActionIds.DISABLE_ORIGIN_TRIALS);
    }
}
