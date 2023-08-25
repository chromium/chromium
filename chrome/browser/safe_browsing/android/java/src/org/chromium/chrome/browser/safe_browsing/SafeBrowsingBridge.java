// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import androidx.annotation.VisibleForTesting;

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

    /**
     * @return The Safe Browsing state. It can be Enhanced Protection, Standard Protection, or No
     *         Protection.
     */
    public static @SafeBrowsingState int getSafeBrowsingState() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingState();
    }

    /**
     * @param state Set the Safe Browsing state. It can be Enhanced Protection, Standard Protection,
     *         or No Protection.
     */
    public static void setSafeBrowsingState(@SafeBrowsingState int state) {
        SafeBrowsingBridgeJni.get().setSafeBrowsingState(state);
    }

    /**
     * @return Whether the Safe Browsing preference is managed. It can be managed by either
     * the SafeBrowsingEnabled policy(legacy) or the SafeBrowsingProtectionLevel policy(new).
     */
    public static boolean isSafeBrowsingManaged() {
        return SafeBrowsingBridgeJni.get().isSafeBrowsingManaged();
    }

    /**
     * @return Whether the user is under Advanced Protection.
     */
    public static boolean isUnderAdvancedProtection() {
        return SafeBrowsingBridgeJni.get().isUnderAdvancedProtection();
    }

    /**
     * @return Whether hash real-time lookup is enabled.
     */
    public static boolean isHashRealTimeLookupEligibleInSession() {
        return SafeBrowsingBridgeJni.get().isHashRealTimeLookupEligibleInSession();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        int umaValueForFile(String path);
        boolean getSafeBrowsingExtendedReportingEnabled();
        void setSafeBrowsingExtendedReportingEnabled(boolean enabled);
        boolean getSafeBrowsingExtendedReportingManaged();
        @SafeBrowsingState
        int getSafeBrowsingState();
        void setSafeBrowsingState(@SafeBrowsingState int state);
        boolean isSafeBrowsingManaged();
        boolean isUnderAdvancedProtection();
        boolean isHashRealTimeLookupEligibleInSession();
    }
}
