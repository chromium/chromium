// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.Bundle;

/**
 * Fake implementation of the Android Credential Manager CreateCredentialResponse object.
 */
public final class FakeAndroidCredManCreateResponse {
    public Bundle getData() {
        Bundle data = new Bundle();
        data.putString("androidx.credentials.BUNDLE_KEY_REGISTRATION_RESPONSE_JSON", "json");
        return data;
    }
}
