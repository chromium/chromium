// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.Nullable;

import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.Relationship;
import org.chromium.components.embedder_support.util.Origin;

import java.util.List;

/**
 * AwOriginVerifier performs OriginVerifications for WebView.
 */
public class AwOriginVerifier extends OriginVerifier {
    public AwOriginVerifier(String packageName, String relationship,
            AwBrowserContext browserContext,
            @Nullable AwVerificationResultStore verificationResultStore) {
        super(packageName, relationship, null, browserContext, verificationResultStore);
    }

    @Override
    public boolean isAllowlisted(String packageName, Origin origin, String relation) {
        return false;
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return wasPreviouslyVerified(mPackageName, mSignatureFingerprints, origin, mRelation);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not trigger an asynchronous
     * validation.
     *
     * @param packageName The package name.
     * @param signatureFingerprint The signatures of the package.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    private static boolean wasPreviouslyVerified(String packageName,
            List<String> signatureFingerprints, Origin origin, String relation) {
        AwVerificationResultStore resultStore = AwVerificationResultStore.getInstance();
        return resultStore.shouldOverride(packageName, origin, relation)
                || resultStore.isRelationshipSaved(
                        new Relationship(packageName, signatureFingerprints, origin, relation));
    }

    @Override
    public void recordResultMetrics(OriginVerifier.VerifierResult result) {
        // TODO(crbug.com/1376958): Implement UMA logging.
    }

    @Override
    public void recordVerificationTimeMetrics(long duration, boolean online) {
        // TODO(crbug.com/1376958): Implement UMA logging.
    }
}
