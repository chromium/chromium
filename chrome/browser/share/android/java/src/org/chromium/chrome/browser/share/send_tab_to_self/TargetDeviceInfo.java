// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.jni_zero.CalledByNative;

import org.chromium.components.sync_device_info.FormFactor;

/**
 * SendTabToSelfEntry mirrors the native struct send_tab_to_self::TargetDeviceInfo declared in
 * //components/send_tab_to_self/target_device_info.h. This provides useful getters and methods
 * called by native code.
 */
class TargetDeviceInfo {
    public final String cacheGuid;
    public final @FormFactor int formFactor;
    public final String deviceName;
    public final long lastUpdatedTimestamp;

    public TargetDeviceInfo(
            String name, String cacheGuid, @FormFactor int formFactor, long lastUpdatedTimestamp) {
        this.deviceName = name;
        this.cacheGuid = cacheGuid;
        this.formFactor = formFactor;
        this.lastUpdatedTimestamp = lastUpdatedTimestamp;
    }

    @CalledByNative
    public static TargetDeviceInfo build(
            String name, String cacheGuid, @FormFactor int formFactor, long lastUpdatedTimestamp) {
        return new TargetDeviceInfo(name, cacheGuid, formFactor, lastUpdatedTimestamp);
    }
}
