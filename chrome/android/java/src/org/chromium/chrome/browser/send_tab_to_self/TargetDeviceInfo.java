// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * SendTabToSelfEntry mirrors the native struct send_tab_to_self::TargetDeviceInfo declared in
 * //components/send_tab_to_self/target_device_info.h. This provides useful getters and methods
 * called by native code.
 */
public class TargetDeviceInfo {
    public final String cacheGuid;
    public final @DeviceType int deviceType;
    public final String deviceName;
    public final long lastUpdatedTimestamp;

    @IntDef({DeviceType.UNSET, DeviceType.WIN, DeviceType.MACOSX, DeviceType.LINUX,
            DeviceType.CHROMEOS, DeviceType.OTHER, DeviceType.PHONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DeviceType {
        // Please keep in sync with components/sync/protocol/sync_enums.proto.
        int UNSET = 0;
        int WIN = 1;
        int MACOSX = 2;
        int LINUX = 3;
        int CHROMEOS = 4;
        int OTHER = 5;
        int PHONE = 6;
    }

    public TargetDeviceInfo(
            String name, String cacheGuid, @DeviceType int deviceType, long lastUpdatedTimestamp) {
        this.deviceName = name;
        this.cacheGuid = cacheGuid;
        this.deviceType = deviceType;
        this.lastUpdatedTimestamp = lastUpdatedTimestamp;
    }

    @CalledByNative
    public static TargetDeviceInfo createTargetDeviceInfo(
            String name, String cacheGuid, @DeviceType int deviceType, long lastUpdatedTimestamp) {
        return new TargetDeviceInfo(name, cacheGuid, deviceType, lastUpdatedTimestamp);
    }
}
