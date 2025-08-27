// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.regional_capabilities;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.regional_capabilities.RegionalProgram;

/** Delegates access to regional programs, per device. */
@NullMarked
public interface RegionalCapabilitiesServiceClientDelegate {

    /**
     * See regional_capabilities::RegionalCapabilitiesService::Client::GetDeviceProgram() for more
     * details.
     */
    @RegionalProgram
    int getDeviceProgram();
}
