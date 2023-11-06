// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.content.Context;
import android.credentials.CreateCredentialException;
import android.credentials.CreateCredentialRequest;
import android.credentials.CreateCredentialResponse;
import android.credentials.CredentialManager;
import android.os.CancellationSignal;
import android.os.OutcomeReceiver;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.concurrent.Executor;

/** Shadow of the CredentialManager object. */
@Implements(value = CredentialManager.class)
public class ShadowCredentialManager {
    private CreateCredentialRequest mRequest;
    private OutcomeReceiver<CreateCredentialResponse, CreateCredentialException>
            mCreateCredentialCallback;

    @Implementation
    protected void __constructor__() {}

    @Implementation
    protected void createCredential(
            Context context,
            CreateCredentialRequest request,
            CancellationSignal cancellationSignal,
            Executor executor,
            OutcomeReceiver<CreateCredentialResponse, CreateCredentialException> callback) {
        mCreateCredentialCallback = callback;
        mRequest = request;
    }

    protected CreateCredentialRequest getCreateCredentialRequest() {
        return mRequest;
    }

    protected OutcomeReceiver<CreateCredentialResponse, CreateCredentialException>
            getCreateCredentialCallback() {
        return mCreateCredentialCallback;
    }
}
