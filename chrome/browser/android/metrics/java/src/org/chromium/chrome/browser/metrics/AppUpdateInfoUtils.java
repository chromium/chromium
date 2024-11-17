// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;

/** Utility class for native to request AppUpdateInfo */
public class AppUpdateInfoUtils {
    private static @Nullable AppUpdateInfoDelegate sDelegate =
            ServiceLoaderUtil.maybeCreate(AppUpdateInfoDelegate.class);

    @CalledByNative
    private static void emitToHistogram() {
        if (sDelegate != null) {
            sDelegate.emitToHistogram();
        }
    }
}
