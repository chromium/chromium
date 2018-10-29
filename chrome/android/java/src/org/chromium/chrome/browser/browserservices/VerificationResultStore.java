// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

import java.util.Collections;
import java.util.Set;

/**
 * Stores the results of Digital Asset Link verifications performed by {@link OriginVerifier}.
 *
 * Lifecycle: This is a utility class with static methods, it won't be instantiated.
 * Thread safety: Methods can be called on any thread.
 */
public class VerificationResultStore {
    /* package */ static void addRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.add(relationship.toString());
        setRelationships(savedLinks);
    }

    /* package */ static void removeRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.remove(relationship.toString());
        setRelationships(savedLinks);
    }

    /* package */ static boolean isRelationshipSaved(Relationship relationship) {
        return getRelationships().contains(relationship.toString());
    }

    /* package */ static void clearStoredRelationships() {
        ThreadUtils.assertOnUiThread();
        setRelationships(Collections.emptySet());
    }

    private static Set<String> getRelationships() {
        // In case we're called on the UI thread and Preferences haven't been read before.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            return ChromePreferenceManager.getInstance().getVerifiedDigitalAssetLinks();
        }
    }

    private static void setRelationships(Set<String> relationships) {
        ChromePreferenceManager.getInstance().setVerifiedDigitalAssetLinks(relationships);
    }

    private VerificationResultStore() {}
}
