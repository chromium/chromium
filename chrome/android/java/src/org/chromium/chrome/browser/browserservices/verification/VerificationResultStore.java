// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores the results of Digital Asset Link verifications performed by {@link OriginVerifier}.
 *
 * Lifecycle: This is a utility class with static methods, it won't be instantiated.
 * Thread safety: Methods can be called on any thread.
 */
public class VerificationResultStore {
    static void addRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.add(relationship.toString());
        setRelationships(savedLinks);
    }

    static void removeRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.remove(relationship.toString());
        setRelationships(savedLinks);
    }

    static boolean isRelationshipSaved(Relationship relationship) {
        return getRelationships().contains(relationship.toString());
    }

    static void clearStoredRelationships() {
        ThreadUtils.assertOnUiThread();
        setRelationships(Collections.emptySet());
    }

    @VisibleForTesting
    static Set<String> getRelationships() {
        // In case we're called on the UI thread and Preferences haven't been read before.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // From the official docs, modifying the result of a SharedPreferences.getStringSet can
            // cause bad things to happen including exceptions or ruining the data.
            return new HashSet<>(SharedPreferencesManager.getInstance().readStringSet(
                    ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS));
        }
    }

    @VisibleForTesting
    static void setRelationships(Set<String> relationships) {
        SharedPreferencesManager.getInstance().writeStringSet(
                ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS, relationships);
    }

    private VerificationResultStore() {}
}
