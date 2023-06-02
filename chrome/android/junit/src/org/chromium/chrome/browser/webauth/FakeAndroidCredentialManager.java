// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.content.Context;
import android.os.CancellationSignal;
import android.os.OutcomeReceiver;

import java.util.concurrent.Executor;

/**
 * Fake implementation of the Android Credential Manager Service.
 */
public final class FakeAndroidCredentialManager {
    FakeAndroidCredManCreateRequest mCreateRequest;
    FakeAndroidCredManGetRequest mGetRequest;
    FakeAndroidCredManException mErrorResponse;
    FakeAndroidCredential mCredential;

    public FakeAndroidCredentialManager() {
        mCredential = new FakeAndroidPublicKeyCredential();
    }

    /**
     * Fake implementation of CredentialManager.createCredential().
     */
    public void createCredential(Context context, FakeAndroidCredManCreateRequest request,
            CancellationSignal cancellationSignal, Executor executor,
            OutcomeReceiver<FakeAndroidCredManCreateResponse, Throwable> callback) {
        mCreateRequest = request;

        if (mErrorResponse != null) {
            callback.onError(mErrorResponse);
            return;
        }
        callback.onResult(new FakeAndroidCredManCreateResponse());
    }

    /**
     * Fake implementation of CredentialManager.getCredential().
     */
    public void getCredential(Context context, FakeAndroidCredManGetRequest request,
            CancellationSignal cancellationSignal, Executor executor,
            OutcomeReceiver<FakeAndroidCredManGetResponse, FakeAndroidCredManException> callback) {
        mGetRequest = request;

        if (mErrorResponse != null) {
            callback.onError(mErrorResponse);
            return;
        }
        callback.onResult(new FakeAndroidCredManGetResponse(mCredential));
    }

    /**
     * Fake implementation of CredentialManager.prepareGetCredential().
     */
    public void prepareGetCredential(FakeAndroidCredManGetRequest request,
            CancellationSignal cancellationSignal, Executor executor,
            OutcomeReceiver<FakeAndroidCredManPrepareGetCredentialResponse,
                    FakeAndroidCredManException> callback) {
        mGetRequest = request;

        if (mErrorResponse != null) {
            callback.onError(mErrorResponse);
            return;
        }
        callback.onResult(new FakeAndroidCredManPrepareGetCredentialResponse());
    }

    /**
     * Returns the received Create Request for inspection by tests.
     */
    public FakeAndroidCredManCreateRequest getCreateRequest() {
        return mCreateRequest;
    }

    /**
     * Returns the received Get Request for inspection by tests.
     */
    public FakeAndroidCredManGetRequest getGetRequest() {
        return mGetRequest;
    }

    /**
     * Sets an error response to be provided to the module under test.
     */
    public void setErrorResponse(FakeAndroidCredManException error) {
        mErrorResponse = error;
    }

    public void setCredManGetResponseCredential(FakeAndroidCredential credential) {
        mCredential = credential;
    }
}
