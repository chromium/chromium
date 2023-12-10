// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Provides access to the C++ multi-platform Safety check code in
 * //components/safety_check.
 */
public class SafetyCheckBridge {
    /** Returns whether the user is signed in for the purposes of password check. */
    static boolean userSignedIn() {
        return SafetyCheckBridgeJni.get().userSignedIn(Profile.getLastUsedRegularProfile());
    }

    /**
     * Triggers the Safe Browsing check on the C++ side.
     *
     * @return SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state
     *     (see //components/safety_check/safety_check.h).
     */
    static @SafeBrowsingStatus int checkSafeBrowsing() {
        return SafetyCheckBridgeJni.get().checkSafeBrowsing(Profile.getLastUsedRegularProfile());
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        boolean userSignedIn(BrowserContextHandle browserContext);

        int checkSafeBrowsing(BrowserContextHandle browserContext);
    }
}
