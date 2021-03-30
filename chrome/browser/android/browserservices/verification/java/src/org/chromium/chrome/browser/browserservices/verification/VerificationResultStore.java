// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores the results of Digital Asset Link verifications performed by {@link OriginVerifier}.
 *
 * There are two types of results stored - proper relationships that are stored in SharedPreferences
 * (and therefore persisted across Chrome launches) and overrides that are stored in a static
 * variable (and therefore not persisted across Chrome launches). Ideally, we will be able to get
 * rid of the overrides in the future, they're just here now for legacy reasons.
 *
 * Lifecycle: This class is a singleton, however you should constructor inject the singleton
 * instance to your classes where possible to make testing easier.
 * Thread safety: Methods can be called on any thread.
 */
public class VerificationResultStore {
    // If we constructed this lazily (creating a new instance in getInstance, that would open us
    // up to a possible race condition if getInstance is called on multiple threads. We could solve
    // this with an AtomicReference, but it seems simpler to just eagerly create the instance.
    private static final VerificationResultStore sInstance = new VerificationResultStore();

    /**
     * A collection of Relationships (stored as Strings, with the signature set to an empty String)
     * that we override verifications to succeed for. It is threadsafe.
     */
    private static final Set<String> sVerificationOverrides =
            Collections.synchronizedSet(new HashSet<>());

    static VerificationResultStore getInstance() {
        return sInstance;
    }

    void addRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.add(relationship.toString());
        setRelationships(savedLinks);
    }

    void removeRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.remove(relationship.toString());
        setRelationships(savedLinks);
    }

    boolean isRelationshipSaved(Relationship relationship) {
        return getRelationships().contains(relationship.toString());
    }

    void addOverride(String packageName, Origin origin, int relationship) {
        sVerificationOverrides.add(overrideToString(packageName, origin, relationship));
    }

    boolean shouldOverride(String packageName, Origin origin, int relationship) {
        return sVerificationOverrides.contains(overrideToString(packageName, origin, relationship));
    }

    private static String overrideToString(String packageName, Origin origin, int relationship) {
        return new Relationship(packageName, "", origin, relationship).toString();
    }

    void clearStoredRelationships() {
        ThreadUtils.assertOnUiThread();
        setRelationships(Collections.emptySet());
        sVerificationOverrides.clear();
    }

    @VisibleForTesting
    Set<String> getRelationships() {
        // In case we're called on the UI thread and Preferences haven't been read before.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // From the official docs, modifying the result of a SharedPreferences.getStringSet can
            // cause bad things to happen including exceptions or ruining the data.
            return new HashSet<>(SharedPreferencesManager.getInstance().readStringSet(
                    ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS));
        }
    }

    @VisibleForTesting
    void setRelationships(Set<String> relationships) {
        SharedPreferencesManager.getInstance().writeStringSet(
                ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS, relationships);
    }
}
