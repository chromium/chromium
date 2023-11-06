// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.Bundle;

/** Fake implementation of the Android Credential Manager Password object. */
public final class FakeAndroidPasswordCredential implements FakeAndroidCredential {
    private final Bundle mDataBundle = new Bundle();

    public FakeAndroidPasswordCredential(String username, String password) {
        mDataBundle.putString("androidx.credentials.BUNDLE_KEY_ID", username);
        mDataBundle.putString("androidx.credentials.BUNDLE_KEY_PASSWORD", password);
    }

    @Override
    public Bundle getData() {
        return mDataBundle;
    }

    @Override
    public String getType() {
        return "android.credentials.TYPE_PASSWORD_CREDENTIAL";
    }
}
