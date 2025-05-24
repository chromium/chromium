// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Util methods for interacting with managed browser (enterprise) state. */
@JNINamespace("enterprise_util")
@NullMarked
public class ManagedBrowserUtils {
    /** Wrapper around native call to determine if the browser is managed. */
    public static boolean isBrowserManaged(Profile profile) {
        return ManagedBrowserUtilsJni.get().isBrowserManaged(profile);
    }

    /** Wrapper around native call to determine if the profile is managed. */
    public static boolean isProfileManaged(Profile profile) {
        return ManagedBrowserUtilsJni.get().isProfileManaged(profile);
    }

    /** Wrapper around native call to get profile manager's representation string. */
    public static String getTitle(Profile profile) {
        return (profile != null) ? ManagedBrowserUtilsJni.get().getTitle(profile) : "";
    }

    /** Wrapper around native call to get if cloud browser reporting is enabled. */
    public static boolean isBrowserReportingEnabled() {
        return ManagedBrowserUtilsJni.get().isBrowserReportingEnabled();
    }

    /** Wrapper around native call to get if cloud profile reporting is enabled. */
    public static boolean isProfileReportingEnabled(Profile profile) {
        return ManagedBrowserUtilsJni.get().isProfileReportingEnabled(profile);
    }

    /** Wrapper around native call to check if "OnSecurityEventEnterpriseConnector" is enabled. */
    public static boolean isOnSecurityEventEnterpriseConnectorEnabled(Profile profile) {
        return ManagedBrowserUtilsJni.get().isOnSecurityEventEnterpriseConnectorEnabled(profile);
    }

    /** Wrapper around native call to check if "EnterpriseRealTimeUrlCheckMode" is enabled. */
    public static boolean isEnterpriseRealTimeUrlCheckModeEnabled(Profile profile) {
        return ManagedBrowserUtilsJni.get().isEnterpriseRealTimeUrlCheckModeEnabled(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean isBrowserManaged(@JniType("Profile*") Profile profile);

        boolean isProfileManaged(@JniType("Profile*") Profile profile);

        @JniType("std::u16string")
        String getTitle(@JniType("Profile*") Profile profile);

        boolean isBrowserReportingEnabled();

        boolean isProfileReportingEnabled(@JniType("Profile*") Profile profile);

        boolean isOnSecurityEventEnterpriseConnectorEnabled(@JniType("Profile*") Profile profile);

        boolean isEnterpriseRealTimeUrlCheckModeEnabled(@JniType("Profile*") Profile profile);
    }
}
