// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.components.webauthn.Fido2CredentialRequest;
import org.chromium.components.webauthn.FidoIntentSender;
import org.chromium.components.webauthn.GpmBrowserOptionsHelper;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * CableAuthenticator implements makeCredential and getAssertion operations on top of the Privileged
 * FIDO2 API.
 */
class CableAuthenticator implements AuthenticationContextProvider {
    private static final String TAG = "CableAuthenticator";

    private static final int REGISTER_REQUEST_CODE = 1;
    private static final int SIGN_REQUEST_CODE = 2;

    private static final int CTAP2_OK = 0;
    private static final int CTAP2_ERR_CREDENTIAL_EXCLUDED = 0x19;
    private static final int CTAP2_ERR_UNSUPPORTED_ALGORITHM = 0x26;
    private static final int CTAP2_ERR_OPERATION_DENIED = 0x27;
    private static final int CTAP2_ERR_UNSUPPORTED_OPTION = 0x2B;
    private static final int CTAP2_ERR_NO_CREDENTIALS = 0x2E;
    private static final int CTAP2_ERR_OTHER = 0x7F;

    private final Context mContext;
    private final CableAuthenticatorUI mUi;
    private final TaskRunner mTaskRunner;
    // mFCMEvent contains the serialized event data that was stored in the notification's
    // PendingIntent.
    private final byte[] mFCMEvent;
    // mServerLinkData contains the information passed from GMS Core in the event that
    // this is a SERVER_LINK connection.
    private final byte[] mServerLinkData;
    // mQRURI contains the contents of a QR code ("FIDO:/234"...), or null if
    // this is not a QR transaction.
    private final String mQRURI;
    // mLinkQR stores whether a QR transaction should send linking information.
    private boolean mLinkQR;

    // mHandle is the opaque ID returned by the native code to ensure that
    // |stop| doesn't apply to a transaction that this instance didn't create.
    private long mHandle;

    public enum Result {
        REGISTER_OK,
        REGISTER_ERROR,
        SIGN_OK,
        SIGN_ERROR,
        OTHER,
    }

    public enum RequestType {
        GET_ASSERTION,
        MAKE_CREDENTIAL,
    }

    public CableAuthenticator(
            Context context,
            CableAuthenticatorUI ui,
            long networkContext,
            long registration,
            byte[] secret,
            boolean isFcmNotification,
            byte[] serverLink,
            byte[] fcmEvent,
            String qrURI) {
        mContext = context;
        mUi = ui;
        mFCMEvent = fcmEvent;
        mServerLinkData = serverLink;
        mQRURI = qrURI;

        // networkContext can only be used from the UI thread, therefore all
        // short-lived work is done on that thread.
        mTaskRunner = PostTask.createTaskRunner(TaskTraits.UI_USER_VISIBLE);
        ThreadUtils.assertOnUiThread();

        WebauthnModeProvider.getInstance().setGlobalWebauthnMode(WebauthnMode.CHROME);
        CableAuthenticatorJni.get().setup(registration, networkContext, secret);

        // Wait for |onTransportReady|.
    }

    // Calls from native code.

    // Called when an informative status update is available. The argument has the same values
    // as the Status enum from v2_authenticator.h.
    @CalledByNative
    public void onStatus(int code) {
        mUi.onStatus(code);
    }

    @CalledByNative
    public static BLEAdvert newBLEAdvert(byte[] payload) {
        return new BLEAdvert(payload);
    }

    @CalledByNative
    public void makeCredential(@JniType("std::vector<uint8_t>") byte[] serializedParams) {
        PublicKeyCredentialCreationOptions params =
                PublicKeyCredentialCreationOptions.deserialize(ByteBuffer.wrap(serializedParams));
        // The Chrome hybrid authenticator never supported creation-time
        // evaluation of PRFs and, by the time we added support in general, we
        // were already in the process of rolling out the hybrid authenticator
        // in Play Services and so it continued not to be supported.
        params.prfInput = null;

        final Fido2CredentialRequest request = new Fido2CredentialRequest(this);
        request.setIsHybridRequest(true);
        final Origin origin = Origin.create(new GURL("https://" + params.relyingParty.id));
        request.handleMakeCredentialRequest(
                params,
                params.challenge,
                GpmBrowserOptionsHelper.createDefaultBrowserOptions(),
                origin,
                origin,
                (status, response) -> {
                    mTaskRunner.execute(
                            () ->
                                    CableAuthenticatorJni.get()
                                            .onAuthenticatorAttestationResponse(
                                                    CTAP2_OK,
                                                    response.attestationObject,
                                                    response.prf));
                    mUi.onAuthenticatorResult(Result.REGISTER_OK);
                },
                (status) -> {
                    final boolean isInvalidStateError =
                            status == AuthenticatorStatus.CREDENTIAL_EXCLUDED;

                    mTaskRunner.execute(
                            () ->
                                    CableAuthenticatorJni.get()
                                            .onAuthenticatorAttestationResponse(
                                                    isInvalidStateError
                                                            ? CTAP2_ERR_CREDENTIAL_EXCLUDED
                                                            : CTAP2_ERR_OPERATION_DENIED,
                                                    null,
                                                    false));

                    mUi.onAuthenticatorResult(
                            isInvalidStateError ? Result.REGISTER_OK : Result.REGISTER_ERROR);
                });
    }

    @CalledByNative
    public void getAssertion(@JniType("std::vector<uint8_t>") byte[] serializedParams) {
        PublicKeyCredentialRequestOptions params =
                PublicKeyCredentialRequestOptions.deserialize(ByteBuffer.wrap(serializedParams));

        final Fido2CredentialRequest request = new Fido2CredentialRequest(this);
        request.setIsHybridRequest(true);
        final Origin origin = Origin.create(new GURL("https://" + params.relyingPartyId));
        request.handleGetAssertionRequest(
                params,
                /* maybeClientDataHash= */ params.challenge,
                origin,
                origin,
                /* payment= */ null,
                (status, response) -> {
                    response.info.clientDataJson = new byte[0];
                    ByteBuffer buffer = response.serialize();
                    byte[] serialized = new byte[buffer.remaining()];
                    buffer.get(serialized);
                    mTaskRunner.execute(
                            () ->
                                    CableAuthenticatorJni.get()
                                            .onAuthenticatorAssertionResponse(
                                                    CTAP2_OK, serialized));
                    mUi.onAuthenticatorResult(Result.SIGN_OK);
                },
                (status) -> {
                    mTaskRunner.execute(
                            () ->
                                    CableAuthenticatorJni.get()
                                            .onAuthenticatorAssertionResponse(
                                                    CTAP2_ERR_OPERATION_DENIED, null));
                    mUi.onAuthenticatorResult(Result.SIGN_ERROR);
                });
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return null;
    }

    @Override
    public FidoIntentSender getIntentSender() {
        return mUi;
    }

    @Override
    public WebContents getWebContents() {
        return null;
    }

    /**
     * Called from native code when a network-based operation has completed.
     *
     * @param ok true if the transaction completed successfully. Otherwise it indicates some form of
     *     error that could include tunnel server errors, handshake failures, etc.
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    @CalledByNative
    public void onComplete(boolean ok, int errorCode) {
        ThreadUtils.assertOnUiThread();
        mUi.onComplete(ok, errorCode);
    }

    void onActivityStop() {
        CableAuthenticatorJni.get().onActivityStop(mHandle);
    }

    void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(TAG, "onActivityResult " + requestCode + " " + resultCode);

        boolean isMakeCredential;
        Result result;
        switch (requestCode) {
            case REGISTER_REQUEST_CODE:
                isMakeCredential = true;
                result = Result.REGISTER_ERROR;
                break;
            case SIGN_REQUEST_CODE:
                isMakeCredential = false;
                result = Result.SIGN_ERROR;
                break;
            default:
                Log.e(TAG, "Ignoring unknown request code " + requestCode);
                return;
        }

        int ctapStatus = CTAP2_ERR_OTHER;
        Object response = null;

        switch (resultCode) {
            case Activity.RESULT_OK:
                if (data == null) {
                    ctapStatus = CTAP2_ERR_OPERATION_DENIED;
                } else {
                    try {
                        response = Fido2Api.parseIntentResponse(data);
                    } catch (IllegalArgumentException e) {
                        response = null;
                    }
                }
                break;

            case Activity.RESULT_CANCELED:
                ctapStatus = CTAP2_ERR_OPERATION_DENIED;
                break;

            default:
                Log.e(TAG, "FIDO2 PendingIntent resulted in code: " + resultCode);
                break;
        }

        if (response == null) {
            // Use already set error code.
        } else if (response instanceof Pair) {
            Pair<Integer, String> error = (Pair<Integer, String>) response;
            Log.e(
                    TAG,
                    "FIDO2 API call resulted in error: "
                            + error.first
                            + " "
                            + (error.second != null ? error.second : ""));

            switch (error.first) {
                case Fido2Api.INVALID_STATE_ERR:
                    if (isMakeCredential) {
                        ctapStatus = CTAP2_ERR_CREDENTIAL_EXCLUDED;
                    } else {
                        ctapStatus = CTAP2_ERR_NO_CREDENTIALS;
                    }
                    break;
                case Fido2Api.NOT_ALLOWED_ERR:
                    if (error.second != null
                            && error.second.equals(
                                    "Request doesn't have a valid list of allowed credentials.")) {
                        ctapStatus = CTAP2_ERR_NO_CREDENTIALS;
                    } else {
                        ctapStatus = CTAP2_ERR_OPERATION_DENIED;
                    }
                    break;
                case Fido2Api.NOT_SUPPORTED_ERR:
                    ctapStatus = CTAP2_ERR_UNSUPPORTED_OPTION;
                    break;
                default:
                    ctapStatus = CTAP2_ERR_OTHER;
                    break;
            }
        } else if (isMakeCredential) {
            if (response instanceof MakeCredentialAuthenticatorResponse) {
                MakeCredentialAuthenticatorResponse r =
                        (MakeCredentialAuthenticatorResponse) response;

                onAuthenticatorAttestationResponse(CTAP2_OK, r.attestationObject, r.prf);
                result = Result.REGISTER_OK;
            }
        } else {
            if (response instanceof GetAssertionAuthenticatorResponse) {
                GetAssertionAuthenticatorResponse gaResponse =
                        (GetAssertionAuthenticatorResponse) response;
                ByteBuffer buffer = gaResponse.serialize();
                byte[] serialized = new byte[buffer.remaining()];
                buffer.get(serialized);
                onAuthenticatorAssertionResponse(CTAP2_OK, serialized);
                result = Result.SIGN_OK;
            }
        }

        if (result != Result.REGISTER_OK && result != Result.SIGN_OK) {
            if (isMakeCredential) {
                onAuthenticatorAttestationResponse(ctapStatus, null, false);
            } else {
                onAuthenticatorAssertionResponse(ctapStatus, null);
            }
        }

        mUi.onAuthenticatorResult(result);
    }

    private void onAuthenticatorAttestationResponse(
            int ctapStatus, byte[] attestationObject, boolean prfEnabled) {
        mTaskRunner.execute(
                () ->
                        CableAuthenticatorJni.get()
                                .onAuthenticatorAttestationResponse(
                                        ctapStatus, attestationObject, prfEnabled));
    }

    private void onAuthenticatorAssertionResponse(int ctapStatus, byte[] responseBytes) {
        mTaskRunner.execute(
                () ->
                        CableAuthenticatorJni.get()
                                .onAuthenticatorAssertionResponse(ctapStatus, responseBytes));
    }

    // Calls from UI.

    void setQRLinking(boolean link) {
        mLinkQR = link;
    }

    /** Called to indicate that Bluetooth transports are ready for processing. */
    void onTransportReady() {
        ThreadUtils.assertOnUiThread();

        if (mServerLinkData != null) {
            mHandle = CableAuthenticatorJni.get().startServerLink(this, mServerLinkData);
        } else if (mQRURI != null) {
            mHandle = CableAuthenticatorJni.get().startQR(this, getName(), mQRURI, mLinkQR);
        } else if (mFCMEvent != null) {
            mHandle = CableAuthenticatorJni.get().startCloudMessage(this, mFCMEvent);
        }
    }

    void close() {
        ThreadUtils.assertOnUiThread();
        CableAuthenticatorJni.get().stop(mHandle);
    }

    String getName() {
        final String name =
                Settings.Global.getString(
                        mContext.getContentResolver(), Settings.Global.DEVICE_NAME);
        if (name != null && name.length() > 0) {
            return name;
        }
        return Build.MANUFACTURER + " " + Build.MODEL;
    }

    /**
     * validateServerLinkData returns zero if |serverLink| is a valid argument for |startServerLink|
     * or else an error value from cablev2::authenticator::Platform::Error.
     */
    static int validateServerLinkData(byte[] serverLinkData) {
        return CableAuthenticatorJni.get().validateServerLinkData(serverLinkData);
    }

    /**
     * validateQRURI returns zero if |uri| is a valid FIDO QR code or else an error value from
     * cablev2::authenticator::Platform::Error.
     */
    static int validateQRURI(String uri) {
        return CableAuthenticatorJni.get().validateQRURI(uri);
    }

    @NativeMethods("cablev2_authenticator")
    interface Natives {
        /**
         * setup is called before any other functions in order for the native code to perform
         * one-time setup operations. It may be called several times, but subsequent calls are
         * ignored.
         */
        void setup(
                long registration,
                long networkContext,
                @JniType("std::vector<uint8_t>") byte[] secret);

        /**
         * Called to instruct the C++ code to start a new transaction based on the contents of a QR
         * code. The given name will be transmitted to the peer in order to identify this device, it
         * should be human-meaningful. The qrURI must be a fido: URI. Returns an opaque value that
         * can be passed to |stop| to cancel this transaction.
         */
        long startQR(
                CableAuthenticator cableAuthenticator,
                @JniType("std::string") String authenticatorName,
                @JniType("std::string") String qrURI,
                boolean link);

        /**
         * Called to instruct the C++ code to start a new transaction based on the given link
         * information which has been provided by the server. Returns an opaque value that can be
         * passed to |stop| to cancel this transaction.
         */
        long startServerLink(
                CableAuthenticator cableAuthenticator,
                @JniType("std::vector<uint8_t>") byte[] serverLinkData);

        /**
         * Called when a GCM message is received and the user has tapped on the resulting
         * notification. fcmEvent contains a serialized event, as created by
         * |webauthn::authenticator::Registration::Event::Serialize|.
         */
        long startCloudMessage(
                CableAuthenticator cableAuthenticator,
                @JniType("std::vector<uint8_t>") byte[] fcmEvent);

        /**
         * Called to alert the C++ code to stop any ongoing transactions. Takes an opaque handle
         * value that was returned by one of the |start*| functions.
         */
        void stop(long handle);

        /**
         * validateServerLinkData returns zero if |serverLink| is a valid argument for
         * |startServerLink| or else an error value from cablev2::authenticator::Platform::Error.
         */
        int validateServerLinkData(@JniType("std::vector<uint8_t>") byte[] serverLinkData);

        /**
         * validateQRURI returns zero if |qrURI| is a valid fido: URI or else an error value from
         * cablev2::authenticator::Platform::Error.
         */
        int validateQRURI(@JniType("std::string") String qrURI);

        /**
         * onActivityStop is called when onStop() is called on the Activity. This is done in order
         * to record events because we want to know when users are abandoning the process.
         */
        void onActivityStop(long handle);

        /** Called to alert native code of a response to a makeCredential request. */
        void onAuthenticatorAttestationResponse(
                int ctapStatus,
                @JniType("std::vector<uint8_t>") byte[] attestationObject,
                boolean prfEnabled);

        /** Called to alert native code of a response to a getAssertion request. */
        void onAuthenticatorAssertionResponse(int ctapStatus, byte[] responseBytes);
    }
}
