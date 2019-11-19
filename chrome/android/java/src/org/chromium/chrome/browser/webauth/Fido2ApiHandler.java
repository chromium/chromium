// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * Android implementation of the Authenticator service defined in
 * //third_party/blink/public/mojom/webauth/authenticator.mojom.
 */
public class Fido2ApiHandler {
    private static Fido2ApiHandler sInstance;
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    private static final int GMSCORE_MIN_VERSION = 12800000;

    @VisibleForTesting
    static void overrideInstanceForTesting(Fido2ApiHandler instance) {
        sInstance = instance;
    }

    /**
     * @return The Fido2ApiHandler for use during the lifetime of the browser process.
     */
    public static Fido2ApiHandler getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            // The Fido2 APIs can only be used on GmsCore v19+.
            // This check is only if sInstance is null since some tests may
            // override sInstance for testing.
            assert PackageUtils.getPackageVersion(
                    ContextUtils.getApplicationContext(), GMSCORE_PACKAGE_NAME)
                    >= GMSCORE_MIN_VERSION;

            sInstance = AppHooks.get().createFido2ApiHandler();
        }
        return sInstance;
    }

    protected void makeCredential(PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, HandlerResponseCallback callback) {}

    protected void getAssertion(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, HandlerResponseCallback callback) {}

    protected void isUserVerifyingPlatformAuthenticatorAvailable(
            RenderFrameHost frameHost, HandlerResponseCallback callback) {}
}
