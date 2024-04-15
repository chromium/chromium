// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Util methods for interacting with managed browser (enterprise) state. */
@JNINamespace("chrome::enterprise_util")
public class ManagedBrowserUtils {
    /** Wrapper around native call to determine if the browser is managed. */
    public static boolean isBrowserManaged(Profile profile) {
        return ManagedBrowserUtilsJni.get().isBrowserManaged(profile);
    }

    /** Wrapper around native call to get profile manager's representation string. */
    public static String getTitle(Profile profile) {
        return (profile != null) ? ManagedBrowserUtilsJni.get().getTitle(profile) : "";
    }

    /** Wrapper around native call to get if cloud reporting is enabled. */
    public static boolean isReportingEnabled() {
        return ManagedBrowserUtilsJni.get().isReportingEnabled();
    }

    @NativeMethods
    public interface Natives {
        boolean isBrowserManaged(@JniType("Profile*") Profile profile);

        String getTitle(@JniType("Profile*") Profile profile);

        boolean isReportingEnabled();
    }
}
