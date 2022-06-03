// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import android.content.pm.PackageInfo;
import android.content.pm.Signature;

import androidx.browser.customtabs.CustomTabsService;

import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.components.embedder_support.util.Origin;

/**
 * Methods that make it easier to unit test functionality relying on {@link OriginVerifier}.
 */
public class OriginVerifierUnitTestSupport {
    // A valid Android package signature - there are no requirements other than it being valid.
    private static final byte[] PACKAGE_SIGNATURE = new byte[] {48, -126, 3, -121, 48, -126, 2, 111,
            -96, 3, 2, 1, 2, 2, 4, 20, -104, -66, -126, 48, 13, 6, 9, 42, -122, 72, -122, -9, 13, 1,
            1, 11, 5, 0, 48, 116, 49, 11, 48, 9, 6, 3, 85, 4, 6, 19, 2, 67, 65, 49, 16, 48, 14, 6,
            3, 85, 4, 8, 19, 7, 79, 110, 116, 97, 114, 105, 111, 49, 17, 48, 15, 6, 3, 85, 4, 7, 19,
            8, 87, 97, 116, 101, 114, 108, 111, 111, 49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8, 67,
            104, 114, 111, 109, 105, 117, 109, 49, 17, 48};

    /**
     * Registers the given package with Robolectric's ShadowPackageManager and provides it with a
     * valid signature, so calls to
     * {@link PackageFingerprintCalculator#getCertificateSHA256FingerprintForPackage} will not
     * crash.
     */
    public static void registerPackageWithSignature(
            ShadowPackageManager shadowPackageManager, String packageName, int uid) {
        PackageInfo info = new PackageInfo();
        info.signatures = new Signature[] {new Signature(PACKAGE_SIGNATURE)};
        info.packageName = packageName;
        shadowPackageManager.addPackage(info);
        shadowPackageManager.setPackagesForUid(uid, packageName);
    }

    /**
     * Registers the given relationship as valid, so future attempts by the OriginVerifier to
     * validate that relationship will pass.
     */
    public static void addVerification(
            String packageName, Origin origin, @CustomTabsService.Relation int relationship) {
        // A more thorough way to override test verification would be to mock out
        // OriginVerifier.Natives. This would mean that most of the logic inside OriginVerifier
        // would also be tested. Unfortunately OriginVerifier relies on native being loaded (it
        // uses Profile.getLastUsedRegularProfile()), so even with the natives mocked out, it would
        // fail to run.

        VerificationResultStore.getInstance().addOverride(packageName, origin, relationship);
    }
}
