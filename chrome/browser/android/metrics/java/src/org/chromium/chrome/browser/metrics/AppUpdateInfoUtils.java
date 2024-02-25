// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

import org.jni_zero.CalledByNative;

/** Utility class for native to request AppUpdateInfo */
public class AppUpdateInfoUtils {
    @CalledByNative
    private static void emitToHistogram() {
        AppUpdateInfo.getInstance().emitToHistogram();
    }
}
