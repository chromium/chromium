// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.fingerprint.FingerprintManager;
import android.os.Build;

import org.chromium.base.PackageUtils;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;

/**
 * Android implementation of the authenticator.mojom interface.
 */
public class AuthenticatorImpl extends HandlerResponseCallback implements Authenticator {
    private final RenderFrameHost mRenderFrameHost;
    private final WebContents mWebContents;

    private static final String GMSCORE_PACKAGE_NAME = "com.google.android.gms";
    private static final int GMSCORE_MIN_VERSION = 12800000;
    private static final int GMSCORE_MIN_VERSION_ISUVPAA = 16200000;

    /** Ensures only one request is processed at a time. */
    private boolean mIsOperationPending;

    private org.chromium.mojo.bindings.Callbacks
            .Callback2<Integer, MakeCredentialAuthenticatorResponse> mMakeCredentialCallback;
    private org.chromium.mojo.bindings.Callbacks
            .Callback2<Integer, GetAssertionAuthenticatorResponse> mGetAssertionCallback;
    private org.chromium.mojo.bindings.Callbacks
            .Callback1<Boolean> mIsUserVerifyingPlatformAuthenticatorAvailableCallback;

    /**
     * Builds the Authenticator service implementation.
     *
     * @param renderFrameHost The host of the frame that has invoked the API.
     */
    public AuthenticatorImpl(RenderFrameHost renderFrameHost) {
        assert renderFrameHost != null;
        mRenderFrameHost = renderFrameHost;
        mWebContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);
    }

    @Override
    public void makeCredential(
            PublicKeyCredentialCreationOptions options, MakeCredentialResponse callback) {
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null);
            return;
        }

        mMakeCredentialCallback = callback;
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME) < GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        mIsOperationPending = true;
        Fido2ApiHandler.getInstance().makeCredential(options, mRenderFrameHost, this);
    }

    @Override
    public void getAssertion(
            PublicKeyCredentialRequestOptions options, GetAssertionResponse callback) {
        if (mIsOperationPending) {
            callback.call(AuthenticatorStatus.PENDING_REQUEST, null);
            return;
        }

        mGetAssertionCallback = callback;
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME) < GMSCORE_MIN_VERSION) {
            onError(AuthenticatorStatus.NOT_IMPLEMENTED);
            return;
        }

        mIsOperationPending = true;
        Fido2ApiHandler.getInstance().getAssertion(options, mRenderFrameHost, this);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.N)
    public void isUserVerifyingPlatformAuthenticatorAvailable(
            IsUserVerifyingPlatformAuthenticatorAvailableResponse callback) {
        Context context = ChromeActivity.fromWebContents(mWebContents);
        // ChromeActivity could be null.
        if (context == null) {
            callback.call(false);
            return;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_AUTH)) {
            callback.call(false);
            return;
        }

        if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                >= GMSCORE_MIN_VERSION_ISUVPAA) {
            mIsUserVerifyingPlatformAuthenticatorAvailableCallback = callback;
            Fido2ApiHandler.getInstance().isUserVerifyingPlatformAuthenticatorAvailable(
                    mRenderFrameHost, this);
        } else if (PackageUtils.getPackageVersion(context, GMSCORE_PACKAGE_NAME)
                        >= GMSCORE_MIN_VERSION
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            FingerprintManager fingerprintManager =
                    (FingerprintManager) context.getSystemService(Context.FINGERPRINT_SERVICE);
            callback.call(
                    fingerprintManager != null && fingerprintManager.hasEnrolledFingerprints());
        } else {
            callback.call(false);
        }
    }

    @Override
    public void cancel() {
        // Not implemented, ignored because request sent to gmscore fido cannot be cancelled.
        return;
    }

    /**
     * Callbacks for receiving responses from the internal handlers.
     */
    @Override
    public void onRegisterResponse(Integer status, MakeCredentialAuthenticatorResponse response) {
        assert mMakeCredentialCallback != null;
        mMakeCredentialCallback.call(status, response);
        close();
    }

    @Override
    public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response) {
        assert mGetAssertionCallback != null;
        mGetAssertionCallback.call(status, response);
        close();
    }

    @Override
    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA) {
        assert mIsUserVerifyingPlatformAuthenticatorAvailableCallback != null;
        mIsUserVerifyingPlatformAuthenticatorAvailableCallback.call(isUVPAA);
        mIsUserVerifyingPlatformAuthenticatorAvailableCallback = null;
    }

    @Override
    public void onError(Integer status) {
        assert ((mMakeCredentialCallback != null && mGetAssertionCallback == null)
                || (mMakeCredentialCallback == null && mGetAssertionCallback != null));
        if (mMakeCredentialCallback != null) {
            mMakeCredentialCallback.call(status, null);
        } else if (mGetAssertionCallback != null) {
            mGetAssertionCallback.call(status, null);
        }
        close();
    }

    @Override
    public void close() {
        mIsOperationPending = false;
        mMakeCredentialCallback = null;
        mGetAssertionCallback = null;
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }
}
