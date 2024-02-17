// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.browser.customtabs.CustomTabsService;

import org.chromium.components.embedder_support.util.Origin;

/**
 * Methods that make it easier to unit test functionality relying on {@link ChromeOriginVerifier}.
 */
public class ChromeOriginVerifierUnitTestSupport {
    /**
     * Registers the given relation as valid, so future attempts by the ChromeOriginVerifier to
     * validate that relation will pass.
     */
    public static void addVerification(
            String packageName, Origin origin, @CustomTabsService.Relation int relation) {
        // A more thorough way to override test verification would be to mock out
        // ChromeOriginVerifier.Natives. This would mean that most of the logic inside
        // ChromeOriginVerifier would also be tested. Unfortunately ChromeOriginVerifier relies on
        // native being loaded (it uses ProfileManager.getLastUsedRegularProfile()), so even with
        // the
        // natives mocked out, it would fail to run.

        ChromeVerificationResultStore.getInstance()
                .addOverride(
                        packageName, origin, ChromeOriginVerifier.relationToRelationship(relation));
    }
}
