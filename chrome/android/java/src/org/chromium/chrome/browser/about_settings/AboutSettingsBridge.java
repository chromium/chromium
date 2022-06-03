// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.about_settings;

import org.chromium.base.annotations.NativeMethods;

/**
 * Bridge providing access to native data about Chrome application and OS.
 */
public class AboutSettingsBridge {
    /**
     * @return Chrome application name and version number.
     */
    public static String getApplicationVersion() {
        return AboutSettingsBridgeJni.get().getApplicationVersion();
    }

    /**
     * @return Android OS version.
     */
    public static String getOSVersion() {
        return AboutSettingsBridgeJni.get().getOSVersion();
    }

    @NativeMethods
    interface Natives {
        String getApplicationVersion();
        String getOSVersion();
    }
}
