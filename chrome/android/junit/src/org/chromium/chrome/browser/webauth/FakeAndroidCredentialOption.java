// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.content.ComponentName;
import android.os.Bundle;
import android.util.ArraySet;

import java.util.Set;

/**
 * Fake implementation of the Android Credential Manager CredentialOption object.
 */
public final class FakeAndroidCredentialOption {
    private final String mType;
    private final Bundle mCredentialRetrievalData;
    private final Bundle mCandidateQueryData;
    private final boolean mIsSystemProviderRequired;
    private final ArraySet<ComponentName> mAllowedProviders;

    public String getType() {
        return mType;
    }

    public Bundle getCredentialRetrievalData() {
        return mCredentialRetrievalData;
    }

    public Bundle getCandidateQueryData() {
        return mCandidateQueryData;
    }

    public boolean isSystemProviderRequired() {
        return mIsSystemProviderRequired;
    }

    public Set<ComponentName> getAllowedProviders() {
        return mAllowedProviders;
    }

    private FakeAndroidCredentialOption(String type, Bundle credentialRetrievalData,
            Bundle candidateQueryData, boolean isSystemProviderRequired,
            ArraySet<ComponentName> allowedProviders) {
        mType = type;
        mCredentialRetrievalData = credentialRetrievalData;
        mCandidateQueryData = candidateQueryData;
        mIsSystemProviderRequired = isSystemProviderRequired;
        mAllowedProviders = allowedProviders;
    }

    /**
     * Builder for FakeAndroidCredentialOption.
     */
    public static final class Builder {
        private String mType;
        private Bundle mCredentialRetrievalData;
        private Bundle mCandidateQueryData;
        private boolean mIsSystemProviderRequired;
        private ArraySet<ComponentName> mAllowedProviders = new ArraySet<>();

        public Builder(String type, Bundle credentialRetrievalData, Bundle candidateQueryData) {
            mType = type;
            mCredentialRetrievalData = credentialRetrievalData;
            mCandidateQueryData = candidateQueryData;
        }

        public Builder setIsSystemProviderRequired(boolean isSystemProviderRequired) {
            mIsSystemProviderRequired = isSystemProviderRequired;
            return this;
        }

        public Builder addAllowedProvider(ComponentName allowedProvider) {
            mAllowedProviders.add(allowedProvider);
            return this;
        }

        public Builder setAllowedProviders(Set<ComponentName> allowedProviders) {
            mAllowedProviders = new ArraySet<>(allowedProviders);
            return this;
        }

        public FakeAndroidCredentialOption build() {
            return new FakeAndroidCredentialOption(mType, mCredentialRetrievalData,
                    mCandidateQueryData, mIsSystemProviderRequired, mAllowedProviders);
        }
    }
}
