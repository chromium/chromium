// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

import java.io.File;

/** Bridge allowing Java code to query details about the login DB deprecation state. */
public class LoginDbDeprecationUtilBridge {

    /**
     * Retrieves the path to the auto-exported passwords CSV.
     *
     * @param profile used to determine the path, since the CSV is saved in the profile folder
     * @return string representing the CSV path.
     */
    public static String getAutoExportCsvFilePath(Profile profile) {
        return LoginDbDeprecationUtilBridgeJni.get().getAutoExportCsvFilePath(profile);
    }

    /**
     * Checks whether there is an auto-exported CSV for this profile.
     *
     * @param profile used to get the path to the CSV.
     * @return true if an auto-exported passwords CSV file exists.
     */
    public static boolean hasPasswordsInCsv(Profile profile) {
        String path = getAutoExportCsvFilePath(profile);
        File file = new File(path);
        return file.exists();
    }

    @NativeMethods
    public interface Natives {
        String getAutoExportCsvFilePath(@JniType("Profile*") Profile profile);
    }
}
