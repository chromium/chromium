// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.about_settings;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** Bridge providing access to native data about Chrome application and OS. */
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
        @JniType("std::string")
        String getApplicationVersion();

        @JniType("std::string")
        String getOSVersion();
    }
}
