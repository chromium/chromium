// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/** Helper for opening the NTP customization side panel. */
@JNINamespace("ntp_customization")
@NullMarked
public final class NtpCustomizationSidePanel {
    private NtpCustomizationSidePanel() {}

    /** Returns whether the NTP customization side panel is enabled. */
    public static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_CUSTOMIZE_WEBUI_ANDROID)
                && DeviceInfo.isDesktop();
    }

    /** Opens the NTP customization side panel for the given tab. */
    public static void show(Tab tab) {
        NtpCustomizationSidePanelJni.get().show(tab);
    }

    @NativeMethods
    interface Natives {
        void show(@JniType("TabAndroid*") Tab tab);
    }
}
