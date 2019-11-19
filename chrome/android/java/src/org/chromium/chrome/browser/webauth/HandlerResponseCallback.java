// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;

/**
 * Callback for receiving responses from an internal handler.
 */
public class HandlerResponseCallback {
    /**
     * Callback for handling the response from a request to register a
     * credential with an authenticator.
     */
    public void onRegisterResponse(Integer status, MakeCredentialAuthenticatorResponse response){};

    /**
     * Callback for handling the response from a request to use a credential
     * to produce a signed assertion.
     */
    public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response){};

    /**
     * Callback for handling response from a request to call
     * isUserVerifyingPlatformAuthenticatorAvailable.
     */
    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA){};

    /**
     * Callback for handling any errors from either register or sign requests.
     */
    public void onError(Integer status){};
}
