// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Bridge providing access to native-side Safe Browsing data.
 */
@JNINamespace("safe_browsing")
public final class SafeBrowsingBridge {
    /**
     * Reports UMA values based on files' extensions.
     *
     * @param path The file path.
     * @return The UMA value for the file.
     */
    public static int umaValueForFile(String path) {
        return SafeBrowsingBridgeJni.get().umaValueForFile(path);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is currently enabled.
     */
    public static boolean isSafeBrowsingExtendedReportingEnabled() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingExtendedReportingEnabled();
    }

    /**
     * @param enabled Whether Safe Browsing Extended Reporting should be enabled.
     */
    public static void setSafeBrowsingExtendedReportingEnabled(boolean enabled) {
        SafeBrowsingBridgeJni.get().setSafeBrowsingExtendedReportingEnabled(enabled);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is managed
     */
    public static boolean isSafeBrowsingExtendedReportingManaged() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingExtendedReportingManaged();
    }

    @NativeMethods
    interface Natives {
        int umaValueForFile(String path);
        boolean getSafeBrowsingExtendedReportingEnabled();
        void setSafeBrowsingExtendedReportingEnabled(boolean enabled);
        boolean getSafeBrowsingExtendedReportingManaged();
    }
}
