// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.os.PowerManager;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in Q (API level 29). These need to exist in a
 * separate class so that Android framework can successfully verify the PowerMonitor class without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.Q)
public final class PowerMonitorForQ {
    private PowerMonitorForQ() {}

    public static void addThermalStatusListener(PowerManager powerManager) {
        powerManager.addThermalStatusListener(
                new PowerManager.OnThermalStatusChangedListener() {
                    @Override
                    public void onThermalStatusChanged(int status) {
                        PowerMonitorJni.get().onThermalStatusChanged(status);
                    }
                });
    }
}
