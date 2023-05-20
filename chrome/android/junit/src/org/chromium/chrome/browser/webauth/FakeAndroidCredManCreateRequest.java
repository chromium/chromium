// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.Bundle;

/**
 * Fake implementation of the Android Credential Manager CreateCredentialRequest object.
 */
public final class FakeAndroidCredManCreateRequest {
    private final String mType;
    private final Bundle mCredentialData;
    private final Bundle mCandidateQueryData;
    private boolean mAlwaysSendAppInfoToProvider;
    private final String mOrigin;

    public String getType() {
        return mType;
    }

    public Bundle getCredentialData() {
        return mCredentialData;
    }

    public Bundle getCandidateQueryData() {
        return mCandidateQueryData;
    }

    public boolean getAlwaysSendAppInfoToProvider() {
        return mAlwaysSendAppInfoToProvider;
    }

    public String getOrigin() {
        return mOrigin;
    }

    private FakeAndroidCredManCreateRequest(String type, Bundle credentialData,
            Bundle candidateQueryData, boolean alwaysSendAppInfoToProvider, String origin) {
        mType = type;
        mCredentialData = credentialData;
        mCandidateQueryData = candidateQueryData;
        mAlwaysSendAppInfoToProvider = alwaysSendAppInfoToProvider;
        mOrigin = origin;
    }

    /**
     * Builder for FakeAndroidCredManCreateRequest.
     */
    public static class Builder {
        private String mType;
        private final Bundle mCredentialData;
        private final Bundle mCandidateQueryData;
        private boolean mAlwaysSendAppInfoToProvider;
        private String mOrigin;

        public Builder(String type, Bundle credentialData, Bundle candidateQueryData) {
            mType = type;
            mCredentialData = credentialData;
            mCandidateQueryData = candidateQueryData;
        }

        public FakeAndroidCredManCreateRequest.Builder setAlwaysSendAppInfoToProvider(
                boolean value) {
            mAlwaysSendAppInfoToProvider = value;
            return this;
        }

        public FakeAndroidCredManCreateRequest.Builder setOrigin(String origin) {
            mOrigin = origin;
            return this;
        }

        public FakeAndroidCredManCreateRequest build() {
            return new FakeAndroidCredManCreateRequest(mType, mCredentialData, mCandidateQueryData,
                    mAlwaysSendAppInfoToProvider, mOrigin);
        }
    }
}
