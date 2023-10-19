// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.Bundle;

import java.util.ArrayList;
import java.util.List;

/** Fake implementation of the Android Credential Manager GetCredentialRequest object. */
public final class FakeAndroidCredManGetRequest {
    private final List<FakeAndroidCredentialOption> mCredentialOptions;
    private final Bundle mData;
    private final boolean mAlwaysSendAppInfoToProvider;
    private String mOrigin;

    public List<FakeAndroidCredentialOption> getCredentialOptions() {
        return mCredentialOptions;
    }

    public Bundle getData() {
        return mData;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public boolean getAlwaysSendAppInfoToProvider() {
        return mAlwaysSendAppInfoToProvider;
    }

    private FakeAndroidCredManGetRequest(
            List<FakeAndroidCredentialOption> credentialOptions,
            Bundle data,
            boolean alwaysSendAppInfoToProvider,
            String origin) {
        mCredentialOptions = credentialOptions;
        mData = data;
        mAlwaysSendAppInfoToProvider = alwaysSendAppInfoToProvider;
        mOrigin = origin;
    }

    /** Builder for FakeAndroidCredManGetRequest. */
    public static final class Builder {
        private List<FakeAndroidCredentialOption> mCredentialOptions = new ArrayList<>();
        private final Bundle mData;
        private String mOrigin;
        private boolean mAlwaysSendAppInfoToProvider = true;

        public Builder(Bundle data) {
            mData = data;
        }

        public Builder addCredentialOption(FakeAndroidCredentialOption credentialOption) {
            mCredentialOptions.add(credentialOption);
            return this;
        }

        public Builder setOrigin(String origin) {
            mOrigin = origin;
            return this;
        }

        public Builder setAlwaysSendAppInfoToProvider(boolean value) {
            mAlwaysSendAppInfoToProvider = value;
            return this;
        }

        public FakeAndroidCredManGetRequest build() {
            return new FakeAndroidCredManGetRequest(
                    mCredentialOptions, mData, mAlwaysSendAppInfoToProvider, mOrigin);
        }
    }
}
