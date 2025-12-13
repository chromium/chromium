// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.battery;

import android.content.Context;
import android.os.PowerManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Wrapper for exposing OS level battery saver mode status. */
@JNINamespace("battery::android")
@NullMarked
public class BatterySaverOSSetting {
    private static boolean sInitialized;
    private static @Nullable PowerManager sPowerManager;

    @CalledByNative
    public static boolean isBatterySaverEnabled() {
        // We don't need to worry about threading since all we do is get a system service.
        if (!sInitialized) {
            sPowerManager =
                    (PowerManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.POWER_SERVICE);
            sInitialized = true;
        }
        return sPowerManager != null && sPowerManager.isPowerSaveMode();
    }
}
