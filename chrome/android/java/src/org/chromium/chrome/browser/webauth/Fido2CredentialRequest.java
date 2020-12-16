// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.fido2.Fido2PendingIntent;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorResponse;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.security.NoSuchAlgorithmException;

/**
 * Uses the Google Play Services Fido2 APIs.
 * Holds the logic of each request.
 */
public class Fido2CredentialRequest implements WindowAndroid.IntentCallback {
    private static final String TAG = "Fido2Request";
    private HandlerResponseCallback mCallback;
    private HandlerResponseCallback mIsUserVerifyingPlatformAuthenticatorAvailableCallback;
    private Fido2PrivilegedApiClient mFido2ApiClient;
    private WebContents mWebContents;
    private ActivityWindowAndroid mWindow;
    private @RequestStatus int mRequestStatus;
    private boolean mAppIdExtensionUsed;
    private long mStartTimeMs;

    @IntDef({
            REGISTER_REQUEST,
            SIGN_REQUEST,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RequestStatus {}

    /** Request status: processing a register request. */
    public static final int REGISTER_REQUEST = 1;

    /** Request status: processing a sign request. */
    public static final int SIGN_REQUEST = 2;

    /** The key used to retrieve PublicKeyCredential. */
    public static final String FIDO2_KEY_CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";

    private void returnErrorAndResetCallback(int error) {
        assert mCallback != null;
        if (mCallback == null) return;
        mCallback.onError(error);
        mCallback = null;
    }

    // Listens for a Fido2PendingIntent.
    private OnSuccessListener<Fido2PendingIntent> mIntentListener = new OnSuccessListener<
            Fido2PendingIntent>() {
        @Override
        public void onSuccess(Fido2PendingIntent fido2PendingIntent) {
            if (!fido2PendingIntent.hasPendingIntent()) {
                Log.e(TAG, "Didn't receive a pending intent.");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                return;
            }

            if (mWindow == null) {
                mWindow = ChromeActivity.fromWebContents(mWebContents).getWindowAndroid();
                if (mWindow == null) {
                    Log.e(TAG, "Couldn't get ActivityWindowAndroid.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                    return;
                }
            }

            final Activity activity = mWindow.getActivity().get();
            if (activity == null) {
                Log.e(TAG, "Null activity.");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                return;
            }

            Callback<Integer> mIntentTrigger = (Integer result) -> {
                try {
                    fido2PendingIntent.launchPendingIntent(activity, result);
                } catch (IntentSender.SendIntentException e) {
                    Log.e(TAG, "Failed to send Fido2 register request to Google Play Services.");
                    returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                }
            };

            // Record starting time that will be used to establish a timeout that will
            // be activated when we receive a response that cannot be returned to the
            // relying party prior to timeout.
            mStartTimeMs = SystemClock.elapsedRealtime();
            int requestCode =
                    mWindow.showCancelableIntent(mIntentTrigger, Fido2CredentialRequest.this, null);

            if (requestCode == WindowAndroid.START_INTENT_FAILURE) {
                Log.e(TAG, "Failed to send Fido2 request to Google Play Services.");
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            } else {
                Log.e(TAG, "Sent a Fido2 request to Google Play Services.");
            }
        }
    };

    public void handleMakeCredentialRequest(
            org.chromium.blink.mojom.PublicKeyCredentialCreationOptions options,
            RenderFrameHost frameHost, Origin origin, HandlerResponseCallback callback) {
        assert mCallback == null;
        mCallback = callback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        mRequestStatus = REGISTER_REQUEST;

        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        int securityCheck = frameHost.performMakeCredentialWebAuthSecurityChecks(
                options.relyingParty.id, origin);
        if (securityCheck != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(securityCheck);
            return;
        }

        PublicKeyCredentialCreationOptions credentialCreationOptions;
        try {
            credentialCreationOptions = Fido2Helper.toMakeCredentialOptions(options);
        } catch (NoSuchAlgorithmException e) {
            returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            return;
        }

        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setOrigin(Uri.parse(convertOriginToString(origin)))
                        .build();

        Task<Fido2PendingIntent> result = mFido2ApiClient.getRegisterIntent(browserRequestOptions);
        result.addOnSuccessListener(mIntentListener);
    }

    public void handleGetAssertionRequest(PublicKeyCredentialRequestOptions options,
            RenderFrameHost frameHost, Origin origin, HandlerResponseCallback callback) {
        assert mCallback == null;
        mCallback = callback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }

        mRequestStatus = SIGN_REQUEST;

        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
            return;
        }

        int securityCheck =
                frameHost.performGetAssertionWebAuthSecurityChecks(options.relyingPartyId, origin);
        if (securityCheck != AuthenticatorStatus.SUCCESS) {
            returnErrorAndResetCallback(securityCheck);
            return;
        }

        if (options.appid != null) {
            mAppIdExtensionUsed = true;
        }

        com.google.android.gms.fido.fido2.api.common
                .PublicKeyCredentialRequestOptions getAssertionOptions;
        getAssertionOptions = Fido2Helper.toGetAssertionOptions(options);

        BrowserPublicKeyCredentialRequestOptions browserRequestOptions =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(getAssertionOptions)
                        .setOrigin(Uri.parse(convertOriginToString(origin)))
                        .build();

        Task<Fido2PendingIntent> result = mFido2ApiClient.getSignIntent(browserRequestOptions);
        result.addOnSuccessListener(mIntentListener);
    }

    public void handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
            RenderFrameHost frameHost, HandlerResponseCallback callback) {
        assert mIsUserVerifyingPlatformAuthenticatorAvailableCallback == null;
        mIsUserVerifyingPlatformAuthenticatorAvailableCallback = callback;
        if (mWebContents == null) {
            mWebContents = WebContentsStatics.fromRenderFrameHost(frameHost);
        }
        if (!initFido2ApiClient()) {
            Log.e(TAG, "Google Play Services' Fido2PrivilegedApi is not available.");
            // Note that |IsUserVerifyingPlatformAuthenticatorAvailable| only returns
            // true or false, making it unable to handle any error status.
            // So it callbacks with false if Fido2PrivilegedApi is not available.
            mIsUserVerifyingPlatformAuthenticatorAvailableCallback
                    .onIsUserVerifyingPlatformAuthenticatorAvailableResponse(false);
            mIsUserVerifyingPlatformAuthenticatorAvailableCallback = null;
            return;
        }

        Task<Boolean> result =
                mFido2ApiClient.isUserVerifyingPlatformAuthenticatorAvailable()
                        .addOnSuccessListener((isUVPAA) -> {
                            mIsUserVerifyingPlatformAuthenticatorAvailableCallback
                                    .onIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                            isUVPAA);
                            mIsUserVerifyingPlatformAuthenticatorAvailableCallback = null;
                        });
    }

    /* Initialize the FIDO2 browser API client. */
    private boolean initFido2ApiClient() {
        if (mFido2ApiClient != null) {
            return true;
        }

        if (!ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                    new UserRecoverableErrorHandler.Silent())) {
            return false;
        }

        mFido2ApiClient = Fido.getFido2PrivilegedApiClient(ContextUtils.getApplicationContext());
        if (mFido2ApiClient == null) {
            return false;
        }
        return true;
    }

    @VisibleForTesting
    protected void setActivityWindowForTesting(ActivityWindowAndroid window) {
        mWindow = window;
    }

    @VisibleForTesting
    protected void setWebContentsForTesting(WebContents webContents) {
        mWebContents = webContents;
    }

    // Handles the result.
    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode, Intent data) {
        if (data == null) {
            Log.e(TAG, "Received a null intent.");
            // The user canceled the request.
            returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
            return;
        }

        switch (resultCode) {
            case Activity.RESULT_CANCELED:
                returnErrorAndResetCallback(AuthenticatorStatus.NOT_ALLOWED_ERROR);
                break;
            case Activity.RESULT_OK:
                processIntentResult(data);
                break;
            default:
                // Something went wrong.
                Log.e(TAG, "Failed with result code" + resultCode);
                returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
                break;
        }
    }

    private void processPublicKeyCredential(Intent data) {
        PublicKeyCredential publicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorResponse response = publicKeyCredential.getResponse();
        if (response instanceof AuthenticatorErrorResponse) {
            processErrorResponse((AuthenticatorErrorResponse) response);
        } else if (response instanceof AuthenticatorAttestationResponse) {
            try {
                mCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS,
                        Fido2Helper.toMakeCredentialResponse(publicKeyCredential));
                mCallback = null;
            } catch (NoSuchAlgorithmException e) {
                returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
            }
        } else if (response instanceof AuthenticatorAssertionResponse) {
            mCallback.onSignResponse(AuthenticatorStatus.SUCCESS,
                    Fido2Helper.toGetAssertionResponse(publicKeyCredential, mAppIdExtensionUsed));
            mCallback = null;
        }
    }

    private void processErrorResponse(AuthenticatorErrorResponse errorResponse) {
        Log.e(TAG,
                "Google Play Services FIDO2 API returned an error: "
                        + errorResponse.getErrorMessage());
        int authenticatorStatus = Fido2Helper.convertError(
                errorResponse.getErrorCode(), errorResponse.getErrorMessage());
        returnErrorAndResetCallback(authenticatorStatus);
    }

    private void processKeyResponse(Intent data) {
        switch (mRequestStatus) {
            case REGISTER_REQUEST:
                Log.e(TAG, "Received a register response from Google Play Services FIDO2 API");
                try {
                    mCallback.onRegisterResponse(AuthenticatorStatus.SUCCESS,
                            Fido2Helper.toMakeCredentialResponse(
                                    AuthenticatorAttestationResponse.deserializeFromBytes(
                                            data.getByteArrayExtra(
                                                    Fido.FIDO2_KEY_RESPONSE_EXTRA))));
                } catch (NoSuchAlgorithmException e) {
                    returnErrorAndResetCallback(AuthenticatorStatus.ALGORITHM_UNSUPPORTED);
                }
                break;
            case SIGN_REQUEST:
                Log.e(TAG, "Received a sign response from Google Play Services FIDO2 API");
                mCallback.onSignResponse(AuthenticatorStatus.SUCCESS,
                        Fido2Helper.toGetAssertionResponse(
                                AuthenticatorAssertionResponse.deserializeFromBytes(
                                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)),
                                mAppIdExtensionUsed));
                break;
        }
        mCallback = null;
    }

    private void processIntentResult(Intent data) {
        // If returned PublicKeyCredential, use PublicKeyCredential to retrieve
        // [Attestation/Assertion/Error] Response, else directly retrieve
        // [Attestation/Assertion/Error] Response.
        if (data.hasExtra(FIDO2_KEY_CREDENTIAL_EXTRA)) {
            processPublicKeyCredential(data);
        } else if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            processErrorResponse(AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA)));
        } else if (data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
            processKeyResponse(data);
        } else {
            // Something went wrong.
            Log.e(TAG,
                    "The response is missing FIDO2_KEY_RESPONSE_EXTRA "
                            + "and FIDO2_KEY_CREDENTIAL_EXTRA.");
            returnErrorAndResetCallback(AuthenticatorStatus.UNKNOWN_ERROR);
        }
    }

    String convertOriginToString(Origin origin) {
        // Wrapping with GURLUtils.getOrigin() in order to trim default ports.
        return GURLUtils.getOrigin(
                origin.getScheme() + "://" + origin.getHost() + ":" + origin.getPort());
    }
}
