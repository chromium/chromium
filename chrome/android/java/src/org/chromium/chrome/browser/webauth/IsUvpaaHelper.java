// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.content.Context;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class provides a separate entry point to the WebAuthentication
 * implementation that allows IsUserVerifyingPlatformAuthenticator to be called
 * without a RenderFrameHost. It exists to support IsUVPAA metrics.
 * This is intended to be used from native code.
 */
@JNINamespace("webauth")
public class IsUvpaaHelper {
    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";

    /**
     * Determine whether a user-verifying platform authenticator is available
     * for WebAuthn.
     */
    @CalledByNative
    public static void isUserVerifyingPlatformAuthenticatorAvailable() {
        Context context = ContextUtils.getApplicationContext();
        if (context == null) {
            IsUvpaaHelperJni.get().onIsUvpaaComplete(false);
            return;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            IsUvpaaHelperJni.get().onIsUvpaaComplete(false);
            return;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_AUTH)) {
            IsUvpaaHelperJni.get().onIsUvpaaComplete(false);
            return;
        }

        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                < Fido2ApiHandler.GMSCORE_MIN_VERSION) {
            IsUvpaaHelperJni.get().onIsUvpaaComplete(false);
            return;
        }

        Fido2ApiHandler.getInstance().isUserVerifyingPlatformAuthenticatorAvailable(
                null, isUVPAA -> IsUvpaaHelperJni.get().onIsUvpaaComplete(isUVPAA));
    }

    @NativeMethods
    interface Natives {
        void onIsUvpaaComplete(boolean available);
    }
}