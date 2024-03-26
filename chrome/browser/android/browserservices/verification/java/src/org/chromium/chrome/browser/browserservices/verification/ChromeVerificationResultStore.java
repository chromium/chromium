// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.content_relationship_verification.VerificationResultStore;

import java.util.HashSet;
import java.util.Set;

/**
 * ChromeVerificationResultStore stores relationships to SharedPreferences which are therefore
 * persisted across Chrome launches.
 */
public class ChromeVerificationResultStore extends VerificationResultStore {
    // If we constructed this lazily (creating a new instance in getInstance, that would open us
    // up to a possible race condition if getInstance is called on multiple threads. We could solve
    // this with an AtomicReference, but it seems simpler to just eagerly create the instance.
    private static final ChromeVerificationResultStore sInstance =
            new ChromeVerificationResultStore();

    private ChromeVerificationResultStore() {}

    public static ChromeVerificationResultStore getInstance() {
        return sInstance;
    }

    @Override
    @VisibleForTesting
    public Set<String> getRelationships() {
        return new HashSet<>(
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS));
    }

    @Override
    @VisibleForTesting
    public void setRelationships(Set<String> relationships) {
        ChromeSharedPreferences.getInstance()
                .writeStringSet(ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS, relationships);
    }

    public static ChromeVerificationResultStore getInstanceForTesting() {
        return getInstance();
    }
}
