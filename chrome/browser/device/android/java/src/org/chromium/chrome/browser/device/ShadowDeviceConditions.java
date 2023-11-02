// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device;

import android.content.Context;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.net.ConnectionType;

/** Custom shadow for the OfflinePageUtils. */
@Implements(DeviceConditions.class)
public class ShadowDeviceConditions {
    /** Device conditions for testing. */
    private static DeviceConditions sDeviceConditions = new DeviceConditions();

    /** Sets device conditions that will be used in test. */
    public static void setCurrentConditions(DeviceConditions deviceConditions) {
        sDeviceConditions = deviceConditions;
    }

    /**
     * Sets current connection type for the device conditions that will be
     * used for tests.
     */
    public static void setCurrentNetworkConnectionType(@ConnectionType int connectionType) {
        sDeviceConditions.setNetworkConnectionType(connectionType);
    }

    @Implementation
    public static DeviceConditions getCurrent(Context context) {
        return sDeviceConditions;
    }

    @Implementation
    public static boolean isCurrentlyPowerConnected(Context context) {
        return sDeviceConditions.isPowerConnected();
    }

    @Implementation
    public static int getCurrentBatteryPercentage(Context context) {
        return sDeviceConditions.getBatteryPercentage();
    }

    @Implementation
    public static boolean isCurrentlyInPowerSaveMode(Context context) {
        return sDeviceConditions.isInPowerSaveMode();
    }

    @Implementation
    public static int getCurrentNetConnectionType(Context context) {
        return sDeviceConditions.getNetConnectionType();
    }

    @Implementation
    public static boolean isCurrentActiveNetworkMetered(Context context) {
        return sDeviceConditions.isActiveNetworkMetered();
    }

    @Implementation
    public static boolean isCurrentlyScreenOnAndUnlocked(Context context) {
        return sDeviceConditions.isScreenOnAndUnlocked();
    }
}
