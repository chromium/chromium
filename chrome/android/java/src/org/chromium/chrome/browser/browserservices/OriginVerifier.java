// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.components.embedder_support.util.Origin;

/**
 * This class forwards calls to
 * {@link org.chromium.chrome.browser.browserservices.verification.OriginVerifier}. It will only
 * exist temporarily to make downstream not break while moving that class.
 */
public class OriginVerifier {
    /** See ...verification.OriginVerifier#clearCachedVerificationsForTesting. */
    @VisibleForTesting
    public static void clearCachedVerificationsForTesting() {
        org.chromium.chrome.browser.browserservices.verification.OriginVerifier
                .clearCachedVerificationsForTesting();
    }

    /** See ...verification.OriginVerifier#wasPreviouslyVerified. */
    public static boolean wasPreviouslyVerified(
            String packageName, Origin origin, @CustomTabsService.Relation int relation) {
        return org.chromium.chrome.browser.browserservices.verification.OriginVerifier
                .wasPreviouslyVerified(packageName, origin, relation);
    }

    /** See ...verification.OriginVerifier#addVerificationOverride. */
    public static void addVerificationOverride(
            String packageName, Origin origin, int relationship) {
        org.chromium.chrome.browser.browserservices.verification.OriginVerifier
                .addVerificationOverride(packageName, origin, relationship);
    }
}
