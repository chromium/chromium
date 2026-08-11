// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;

/**
 * Public {@link DeviceResourceProvider} implementation. Downstream may provide a different
 * implementation.
 */
@NullMarked
class DeviceResourceProviderUpstreamImpl implements DeviceResourceProvider {
    @Override
    public int getDeviceTypeIcon(@FormFactor int formFactor, @OsType int osType) {
        switch (formFactor) {
            case FormFactor.DESKTOP:
                return R.drawable.computer_black_24dp;
            case FormFactor.PHONE:
                return R.drawable.smartphone_black_24dp;
            case FormFactor.TABLET:
                return R.drawable.tablet_black_24dp;
            default:
                return R.drawable.devices_black_24dp;
        }
    }
}
