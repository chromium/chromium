// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Records SharedPreferences related to the browserservices module. */
public class BrowserServicesStore {
    private BrowserServicesStore() {}

    /**
     * Sets that the user has accepted the Trusted Web Activity "Running in Chrome" disclosure for
     * TWAs launched by the given package.
     */
    public static void setUserAcceptedTwaDisclosureForPackage(String packageName) {
        ChromeSharedPreferences.getInstance()
                .addToStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES, packageName);
    }

    /**
     * Removes the records of accepting and of originally seeing the TWA "Running in Chrome"
     * disclosure for the given package.
     */
    public static void removeTwaDisclosureAcceptanceForPackage(String packageName) {
        ChromeSharedPreferences.getInstance()
                .removeFromStringSet(
                        ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES, packageName);
        ChromeSharedPreferences.getInstance()
                .removeFromStringSet(
                        ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES, packageName);
    }

    /**
     * Checks whether the given package was previously passed to {@link
     * #setUserAcceptedTwaDisclosureForPackage(String)}.
     */
    public static boolean hasUserAcceptedTwaDisclosureForPackage(String packageName) {
        return ChromeSharedPreferences.getInstance()
                .readStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES)
                .contains(packageName);
    }

    /** Sets that the user has seen the disclosure. */
    public static void setUserSeenTwaDisclosureForPackage(String packageName) {
        ChromeSharedPreferences.getInstance()
                .addToStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES, packageName);
    }

    /** Checks whether the user has seen the disclosure. */
    public static boolean hasUserSeenTwaDisclosureForPackage(String packageName) {
        return ChromeSharedPreferences.getInstance()
                .readStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES)
                .contains(packageName);
    }
}
