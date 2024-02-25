// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.NonNull;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Provides access to the C++ multi-platform Safety check code in
 * //components/safety_check.
 */
public class SafetyCheckBridge {

    private final Profile mProfile;

    /** Constructs a SafetyCheckBridge for a given {@link Profile}. */
    public SafetyCheckBridge(@NonNull Profile profile) {
        mProfile = profile;
    }

    /** Returns whether the user is signed in for the purposes of password check. */
    boolean userSignedIn() {
        return SafetyCheckBridgeJni.get().userSignedIn(mProfile);
    }

    /**
     * Triggers the Safe Browsing check on the C++ side.
     *
     * @return SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state (see
     *     //components/safety_check/safety_check.h).
     */
    @SafeBrowsingStatus
    int checkSafeBrowsing() {
        return SafetyCheckBridgeJni.get().checkSafeBrowsing(mProfile);
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        boolean userSignedIn(BrowserContextHandle browserContext);

        int checkSafeBrowsing(BrowserContextHandle browserContext);
    }
}
