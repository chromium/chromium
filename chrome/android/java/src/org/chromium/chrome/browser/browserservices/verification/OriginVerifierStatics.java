// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import android.content.pm.PackageManager;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.ContextUtils;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Adds a layer of indirection to calls to static methods on {@link OriginVerifier}. This allows us
 * to modify that class a bit more freely without having to worry about downstream relying on its
 * static methods.
 *
 * This should be temporary, see https://crbug.com/1164866
 */
public class OriginVerifierStatics {
    /** Calls {@link OriginVerifier#clearCachedVerificationsForTesting}. */
    @VisibleForTesting
    public static void clearCachedVerificationsForTesting() {
        OriginVerifier.clearCachedVerificationsForTesting();
    }

    /** Calls {@link OriginVerifier#addVerificationOverride}. */
    public static void addVerificationOverride(
            String packageName, Origin origin, int relationship) {
        OriginVerifier.addVerificationOverride(packageName, origin, relationship);
    }

    /** Calls {@link OriginVerifier#wasPreviouslyVerified(String, Origin, int)}. */
    public static boolean wasPreviouslyVerified(
            String packageName, Origin origin, @CustomTabsService.Relation int relation) {
        return OriginVerifier.wasPreviouslyVerified(packageName, origin, relation);
    }

    /** Calls {@link PackageFingerprintCalculator#getCertificateSHA256FingerprintForPackage}. */
    public static String getCertificateSHA256FingerprintForPackage(String packageName) {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        return PackageFingerprintCalculator.getCertificateSHA256FingerprintForPackage(
                pm, packageName);
    }
}
