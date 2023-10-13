// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.url.GURL;

/**
 * Static methods for use in tests that require interacting with federated identity content
 * settings.
 */
@JNINamespace("federated_identity")
public class FederatedIdentityTestUtils {
    // Places FedCM permission under embargo for the passed-in relying party {@link url}.
    public static void embargoFedCmForRelyingParty(GURL url) {
        ThreadUtils.assertOnUiThread();
        FederatedIdentityTestUtilsJni.get().embargoFedCmForRelyingParty(url);
    }

    @NativeMethods
    interface Natives {
        void embargoFedCmForRelyingParty(GURL url);
    }
}
