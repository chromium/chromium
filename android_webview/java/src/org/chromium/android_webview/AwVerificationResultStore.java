// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.components.content_relationship_verification.VerificationResultStore;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * AwVerificationResultStore stores relationships in a local variable.
 */
public class AwVerificationResultStore extends VerificationResultStore {
    private static final AwVerificationResultStore sInstance = new AwVerificationResultStore();

    private Set<String> mVerifiedOrigins = Collections.synchronizedSet(new HashSet<>());

    private AwVerificationResultStore() {}

    public static AwVerificationResultStore getInstance() {
        return sInstance;
    }

    @Override
    protected Set<String> getRelationships() {
        return mVerifiedOrigins;
    }

    @Override
    protected void setRelationships(Set<String> relationships) {
        mVerifiedOrigins = relationships;
    }
}
