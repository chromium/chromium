// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.battery;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;

/**
 * Wrapper for exposing OS level battery saver mode status.
 */
@JNINamespace("battery::android")
public class BatterySaverOSSetting {
    @CalledByNative
    public static boolean isBatterySaverEnabled() {
        return PowerSavingModeMonitor.getInstance().powerSavingIsOn();
    }
}
