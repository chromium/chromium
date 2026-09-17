// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import org.chromium.base.Promise;
import org.chromium.components.embedder_support.util.Origin;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** A {@link Verifier} for testing. */
class TestVerifier implements Verifier {
    private final Set<Origin> mPreviouslyVerifiedOrigins = new HashSet<>();
    private final Map<Origin, Promise<Boolean>> mPendingVerifications = new HashMap<>();

    public void passVerification(Origin origin) {
        completeVerification(origin, true);
    }

    public void failVerification(Origin origin) {
        completeVerification(origin, false);
    }

    private void completeVerification(Origin origin, boolean success) {
        if (mPendingVerifications.get(origin) == null) return;

        mPendingVerifications.get(origin).fulfill(success);
        mPendingVerifications.remove(origin);

        if (success) mPreviouslyVerifiedOrigins.add(origin);
    }

    public boolean hasPendingVerification(Origin origin) {
        return mPendingVerifications.containsKey(origin);
    }

    @Override
    public Promise<Boolean> verify(String url) {
        Origin origin = Origin.create(url);
        // Mirrors TwaVerifier: non-HTTP(S) URLs are not supported and cannot be verified.
        if (origin == null) return Promise.fulfilled(false);

        Promise<Boolean> promise = new Promise<>();
        mPendingVerifications.put(origin, promise);
        return promise;
    }

    @Override
    public boolean wasPreviouslyVerified(String url) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getVerifiedScope(String url) {
        Origin origin = Origin.create(url);
        if (origin == null) return url;
        return origin.toString();
    }

    @Override
    public boolean isUrlInVerifiedScope(String url) {
        throw new UnsupportedOperationException();
    }
}
