// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

import javax.inject.Inject;

/** Records SharedPreferences related to the browserservices module. */
@ActivityScope
public class BrowserServicesStore {
    private final SharedPreferencesManager mManager;

    @Inject
    public BrowserServicesStore(SharedPreferencesManager manager) {
        mManager = manager;
    }

    /**
     * Sets that the user has accepted the Trusted Web Activity "Running in Chrome" disclosure for
     * TWAs launched by the given package.
     */
    public void setUserAcceptedTwaDisclosureForPackage(String packageName) {
        mManager.addToStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES, packageName);
    }

    /**
     * Removes the records of accepting and of originally seeing the TWA "Running in Chrome"
     * disclosure for the given package.
     */
    public void removeTwaDisclosureAcceptanceForPackage(String packageName) {
        mManager.removeFromStringSet(
                ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES, packageName);
        mManager.removeFromStringSet(
                ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES, packageName);
    }

    /**
     * Checks whether the given package was previously passed to
     * {@link #setUserAcceptedTwaDisclosureForPackage(String)}.
     */
    public boolean hasUserAcceptedTwaDisclosureForPackage(String packageName) {
        return mManager.readStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES)
                .contains(packageName);
    }

    /** Sets that the user has seen the disclosure. */
    public void setUserSeenTwaDisclosureForPackage(String packageName) {
        mManager.addToStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES, packageName);
    }

    /** Checks whether the user has seen the disclosure. */
    public boolean hasUserSeenTwaDisclosureForPackage(String packageName) {
        return mManager.readStringSet(ChromePreferenceKeys.TWA_DISCLOSURE_SEEN_PACKAGES)
                .contains(packageName);
    }
}
