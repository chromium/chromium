// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.File;

/** Bridge allowing Java code to query details about the login DB deprecation state. */
@NullMarked
public class LoginDbDeprecationUtilBridge {

    @Nullable private static Boolean sHasCsvFileForTesting;

    /** For testing only. */
    public static void setHasCsvFileForTesting(boolean hasFileForTesting) {
        sHasCsvFileForTesting = hasFileForTesting;
        ResettersForTesting.register(() -> sHasCsvFileForTesting = null);
    }

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
        if (sHasCsvFileForTesting != null) {
            return sHasCsvFileForTesting;
        }
        return new File(getAutoExportCsvFilePath(profile)).exists();
    }

    @NativeMethods
    public interface Natives {
        String getAutoExportCsvFilePath(@JniType("Profile*") Profile profile);
    }
}
