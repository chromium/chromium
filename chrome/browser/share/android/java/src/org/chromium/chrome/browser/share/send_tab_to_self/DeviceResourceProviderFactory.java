// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;

/** Factory for creating {@link DeviceResourceProvider}. */
@NullMarked
public class DeviceResourceProviderFactory {
    /**
     * Creates an instance of {@link DeviceResourceProvider}.
     *
     * @return {@link DeviceResourceProvider}
     */
    public static DeviceResourceProvider create() {
        DeviceResourceProvider provider =
                ServiceLoaderUtil.maybeCreate(DeviceResourceProvider.class);
        if (provider == null) {
            provider = new DeviceResourceProviderUpstreamImpl();
        }
        return provider;
    }
}
