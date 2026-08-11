// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;

/** Interface that provides drawable resource IDs for target devices. */
@NullMarked
public interface DeviceResourceProvider {
    /**
     * Returns the drawable resource ID for a target device based on its form factor and OS type.
     *
     * @param formFactor The device's {@link FormFactor}
     * @param osType The device's {@link OsType}
     * @return The resource ID of the drawable icon.
     */
    @DrawableRes
    int getDeviceTypeIcon(@FormFactor int formFactor, @OsType int osType);
}
