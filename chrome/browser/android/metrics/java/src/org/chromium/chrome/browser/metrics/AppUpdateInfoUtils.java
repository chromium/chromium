// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility class for native to request AppUpdateInfo */
@NullMarked
public class AppUpdateInfoUtils {
    private static final @Nullable AppUpdateInfoDelegate sDelegate =
            ServiceLoaderUtil.maybeCreate(AppUpdateInfoDelegate.class);

    @CalledByNative
    private static void emitToHistogram() {
        if (sDelegate != null) {
            sDelegate.emitToHistogram();
        }
    }
}
