// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ResultReceiver;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.components.webauthn.Fido2ApiCall.Fido2ApiCallParams;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.WebauthnCredentialDetails;
import org.chromium.components.webauthn.WebauthnModeProvider;

import java.security.NoSuchAlgorithmException;
import java.util.List;

/** Mock implementation of {@link Fido2ApiCallHelper} for WebAuthn unit tests. */
@NullMarked
public class MockFido2ApiCallHelper extends Fido2ApiCallHelper {
    private @Nullable List<WebauthnCredentialDetails> mReturnedCredentialDetails;
    private boolean mInvokeCallbackImmediately = true;
    private @Nullable OnSuccessListener<List<WebauthnCredentialDetails>> mSuccessCallback;

    @Override
    public void invokeFido2GetCredentials(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingPartyId,
            OnSuccessListener<List<WebauthnCredentialDetails>> successCallback,
            OnFailureListener failureCallback) {
        if (mInvokeCallbackImmediately) {
            assert mReturnedCredentialDetails != null;
            successCallback.onSuccess(mReturnedCredentialDetails);
            return;
        }
        mSuccessCallback = successCallback;
    }

    @Override
    public void invokePasskeyCacheGetCredentials(
            AuthenticationContextProvider authenticationContextProvider,
            String relyingParty,
            OnSuccessListener<List<WebauthnCredentialDetails>> successListener,
            OnFailureListener failureListener) {
        if (mInvokeCallbackImmediately) {
            assert mReturnedCredentialDetails != null;
            successListener.onSuccess(mReturnedCredentialDetails);
            return;
        }
        mSuccessCallback = successListener;
    }

    @Override
    public void invokeFido2MakeCredential(
            AuthenticationContextProvider authenticationContextProvider,
            PublicKeyCredentialCreationOptions options,
            Uri uri,
            byte @Nullable [] clientDataHash,
            @Nullable Bundle browserOptions,
            @Nullable ResultReceiver resultReceiver,
            OnSuccessListener<PendingIntent> successCallback,
            OnFailureListener failureCallback)
            throws NoSuchAlgorithmException {
        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(authenticationContextProvider.getWebContents());
        assert params != null;
        Parcel args = Parcel.obtain();
        try {
            assumeNonNull(params.mMethodInterfaces);
            params.mMethodInterfaces.makeCredential(
                    options, uri, clientDataHash, browserOptions, resultReceiver, args);
        } finally {
            args.recycle();
        }
        // Don't make any actual calls to Play Services; pass a placeholder PendingIntent to
        // MockIntentSender.
        assert authenticationContextProvider.getContext() != null;
        successCallback.onSuccess(
                PendingIntent.getActivity(
                        authenticationContextProvider.getContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE));
    }

    @Override
    public void invokeFido2GetAssertion(
            AuthenticationContextProvider authenticationContextProvider,
            PublicKeyCredentialRequestOptions options,
            Uri uri,
            byte @Nullable [] clientDataHash,
            @Nullable ResultReceiver resultReceiver,
            OnSuccessListener<PendingIntent> successCallback,
            OnFailureListener failureCallback) {
        Fido2ApiCallParams params =
                WebauthnModeProvider.getInstance()
                        .getFido2ApiCallParams(authenticationContextProvider.getWebContents());
        assert params != null;
        Parcel args = Parcel.obtain();
        try {
            assumeNonNull(params.mMethodInterfaces);
            params.mMethodInterfaces.getAssertion(
                    options, uri, clientDataHash, /* tunnelId= */ null, resultReceiver, args);
        } finally {
            args.recycle();
        }
        // Don't make any actual calls to Play Services; pass a placeholder PendingIntent to
        // MockIntentSender.
        assert authenticationContextProvider.getContext() != null;
        successCallback.onSuccess(
                PendingIntent.getActivity(
                        authenticationContextProvider.getContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE));
    }

    @Override
    public void invokeFido2HybridGetAssertion(
            AuthenticationContextProvider authenticationContextProvider,
            PublicKeyCredentialRequestOptions options,
            Uri uri,
            byte @Nullable [] clientDataHash,
            OnSuccessListener<PendingIntent> successCallback,
            OnFailureListener failureCallback) {
        Parcel args = Parcel.obtain();
        try {
            Fido2Api.appendBrowserGetAssertionOptionsToParcel(
                    options,
                    uri,
                    clientDataHash,
                    /* tunnelId= */ null,
                    /* resultReceiver= */ null,
                    args);
        } finally {
            args.recycle();
        }
        // Don't make any actual calls to Play Services; pass a placeholder PendingIntent to
        // MockIntentSender.
        assert authenticationContextProvider.getContext() != null;
        successCallback.onSuccess(
                PendingIntent.getActivity(
                        authenticationContextProvider.getContext(),
                        0,
                        new Intent(),
                        PendingIntent.FLAG_IMMUTABLE));
    }

    @Override
    public boolean arePlayServicesAvailable() {
        return true;
    }

    public void setReturnedCredentialDetails(List<WebauthnCredentialDetails> details) {
        mReturnedCredentialDetails = details;
    }

    public void setInvokeCallbackImmediately(boolean invokeImmediately) {
        mInvokeCallbackImmediately = invokeImmediately;
    }

    public void invokeSuccessCallback() {
        assert mSuccessCallback != null;
        assert mReturnedCredentialDetails != null;
        mSuccessCallback.onSuccess(mReturnedCredentialDetails);
    }
}
