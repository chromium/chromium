// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Bridge providing access to native-side Safe Browsing data. */
@JNINamespace("safe_browsing")
public final class SafeBrowsingBridge {
    private final Profile mProfile;

    /** Constructs a {@link SafeBrowsingBridge} associated with the given {@link Profile}. */
    public SafeBrowsingBridge(Profile profile) {
        mProfile = profile;
    }

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
    public boolean isSafeBrowsingExtendedReportingEnabled() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingExtendedReportingEnabled(mProfile);
    }

    /**
     * @param enabled Whether Safe Browsing Extended Reporting should be enabled.
     */
    public void setSafeBrowsingExtendedReportingEnabled(boolean enabled) {
        SafeBrowsingBridgeJni.get().setSafeBrowsingExtendedReportingEnabled(mProfile, enabled);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is managed
     */
    public boolean isSafeBrowsingExtendedReportingManaged() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingExtendedReportingManaged(mProfile);
    }

    /**
     * @return The Safe Browsing state. It can be Enhanced Protection, Standard Protection, or No
     *     Protection.
     */
    public @SafeBrowsingState int getSafeBrowsingState() {
        return SafeBrowsingBridgeJni.get().getSafeBrowsingState(mProfile);
    }

    /**
     * @param state Set the Safe Browsing state. It can be Enhanced Protection, Standard Protection,
     *     or No Protection.
     */
    public void setSafeBrowsingState(@SafeBrowsingState int state) {
        SafeBrowsingBridgeJni.get().setSafeBrowsingState(mProfile, state);
    }

    /**
     * @return Whether the Safe Browsing preference is managed. It can be managed by either the
     *     SafeBrowsingEnabled policy(legacy) or the SafeBrowsingProtectionLevel policy(new).
     */
    public boolean isSafeBrowsingManaged() {
        return SafeBrowsingBridgeJni.get().isSafeBrowsingManaged(mProfile);
    }

    /**
     * @return Whether the user is under Advanced Protection.
     */
    public boolean isUnderAdvancedProtection() {
        return SafeBrowsingBridgeJni.get().isUnderAdvancedProtection(mProfile);
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

        boolean getSafeBrowsingExtendedReportingEnabled(Profile profile);

        void setSafeBrowsingExtendedReportingEnabled(Profile profile, boolean enabled);

        boolean getSafeBrowsingExtendedReportingManaged(Profile profile);

        @SafeBrowsingState
        int getSafeBrowsingState(Profile profile);

        void setSafeBrowsingState(Profile profile, @SafeBrowsingState int state);

        boolean isSafeBrowsingManaged(Profile profile);

        boolean isUnderAdvancedProtection(Profile profile);

        boolean isHashRealTimeLookupEligibleInSession();
    }
}
