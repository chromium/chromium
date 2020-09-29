// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.common.Transport;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRpEntity;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialType;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialUserEntity;
import com.google.android.gms.tasks.Task;

import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * CableAuthenticator implements makeCredential and getAssertion operations on top of the Privileged
 * FIDO2 API.
 */
class CableAuthenticator {
    private static final String TAG = "CableAuthenticator";
    private static final String FIDO2_KEY_CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";
    private static final double TIMEOUT_SECONDS = 20;

    private static final int REGISTER_REQUEST_CODE = 1;
    private static final int SIGN_REQUEST_CODE = 2;

    private static final int CTAP2_OK = 0;
    private static final int CTAP2_ERR_OPERATION_DENIED = 0x27;
    private static final int CTAP2_ERR_UNSUPPORTED_OPTION = 0x2D;
    private static final int CTAP2_ERR_OTHER = 0x7F;

    // The filename and key name of the SharedPreferences value that contains
    // the base64-encoded state from the native code.
    private static final String STATE_FILE_NAME = "cablev2_authenticator";
    private static final String STATE_VALUE_NAME = "keys";

    private final Context mContext;
    private final CableAuthenticatorUI mUi;
    private final Callback mCallback;
    private final BLEHandler mBleHandler;
    private final SingleThreadTaskRunner mTaskRunner;
    private boolean mBleStarted;

    public enum Result {
        REGISTER_OK,
        REGISTER_ERROR,
        SIGN_OK,
        SIGN_ERROR,
        OTHER,
    }

    // Callbacks notifying the UI of certain CableAuthenticator state changes. These may run on any
    // thread.
    public interface Callback {
        // Invoked when the authenticator has completed a handshake with a client device.
        void onAuthenticatorConnected();
        // Invoked when a transaction has completed. The response may still be transmitting and
        // onComplete will follow.
        void onAuthenticatorResult(Result result);
        // Invoked when the authenticator has finished. The UI should be dismissed at this point.
        void onComplete();
    }

    public CableAuthenticator(Context context, CableAuthenticatorUI ui) {
        mContext = context;
        mUi = ui;
        mCallback = ui;

        SharedPreferences prefs =
                mContext.getSharedPreferences(STATE_FILE_NAME, Context.MODE_PRIVATE);
        byte[] stateBytes = null;
        try {
            stateBytes = Base64.decode(prefs.getString(STATE_VALUE_NAME, ""), Base64.DEFAULT);
            if (stateBytes.length == 0) {
                stateBytes = null;
            }
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Ignoring corrupt state");
        }

        // All native code runs on this thread to avoid worrying about JNIEnv
        // objects and references being incorrectly used across threads.
        // TODO: in practice, this sadly appears to return the UI thread,
        // despite requesting |BEST_EFFORT|.
        mTaskRunner = PostTask.createSingleThreadTaskRunner(TaskTraits.BEST_EFFORT);

        mBleHandler = new BLEHandler(this, mTaskRunner);

        // Local variables passed into a lambda must be final.
        final byte[] state = stateBytes;
        mTaskRunner.postTask(() -> CableAuthenticatorJni.get().start(this, state));

        if (!mBleHandler.start()) {
            // TODO: handle the case where exporting the GATT server fails.
        }
    }

    // Calls from handler classes.

    /**
     * Called by BLEHandler to indicate that a peer has connected.
     */
    public void notifyAuthenticatorConnected() {
        mCallback.onAuthenticatorConnected();
    }

    /**
     * Called by BLEHandler to signal that a BLE peer wrote data.
     */
    public byte[][] onBLEWrite(long client, int mtu, byte[] payload) {
        assert mTaskRunner.belongsToCurrentThread();

        return CableAuthenticatorJni.get().onBLEWrite(client, mtu, payload);
    }

    /**
     * Called by BLEHandler when transmission of a final reply is complete.
     */
    public void onComplete() {
        mCallback.onComplete();
    }

    /**
     * Called by USBHandler to signal that a USB peer wrote data.
     */
    public byte[] onUSBWrite(byte[] payload) {
        assert mTaskRunner.belongsToCurrentThread();
        // TODO: wire up.
        return null;
    }

    // Calls from native code.

    /**
     * Called by C++ code to start advertising a given UUID, which is passed
     * as 16 bytes.
     */
    public void sendBLEAdvert(byte[] dataUuidBytes) {
        assert mTaskRunner.belongsToCurrentThread();

        if (!mBleStarted && !mBleHandler.start()) {
            // TODO: handle GATT failure.
            return;
        }

        mBleStarted = true;
        mBleHandler.sendBLEAdvert(dataUuidBytes);
    }

    /**
     * Called by native code to store a new state blob.
     */
    public void setState(byte[] newState) {
        assert mTaskRunner.belongsToCurrentThread();

        SharedPreferences prefs =
                mContext.getSharedPreferences(STATE_FILE_NAME, Context.MODE_PRIVATE);
        Log.i(TAG, "Writing updated state");
        prefs.edit()
                .putString(STATE_VALUE_NAME,
                        Base64.encodeToString(newState, Base64.NO_WRAP | Base64.NO_PADDING))
                .apply();
    }

    /**
     * Called by native code to send BLE data to a specified client.
     */
    public void sendNotification(long client, byte[][] fragments, boolean isTransactionEnd) {
        assert mTaskRunner.belongsToCurrentThread();
        assert mBleStarted;

        mBleHandler.sendNotification(client, fragments, /*closeWhenDone=*/isTransactionEnd);
    }

    public void makeCredential(String origin, String rpId, byte[] challenge, byte[] userId,
            int[] algorithms, byte[][] excludedCredentialIds, boolean residentKeyRequired) {
        // TODO: handle concurrent requests
        Fido2PrivilegedApiClient client = Fido.getFido2PrivilegedApiClient(mContext);
        if (client == null) {
            Log.i(TAG, "getFido2PrivilegedApiClient failed");
            return;
        }
        Log.i(TAG, "have fido client");

        List<PublicKeyCredentialParameters> parameters = new ArrayList<>();
        for (int i = 0; i < algorithms.length; i++) {
            try {
                parameters.add(new PublicKeyCredentialParameters(
                        PublicKeyCredentialType.PUBLIC_KEY.toString(), algorithms[i]));
            } catch (IllegalArgumentException e) {
                // The FIDO API will throw IllegalArgumentException for unrecognised algorithms.
                // Since an authenticator ignores unknown algorithms, this exception just needs to
                // be caught and ignored.
            }
        }
        // The GmsCore FIDO2 API does not actually support resident keys yet.
        AuthenticatorSelectionCriteria selection = new AuthenticatorSelectionCriteria.Builder()
                                                           .setAttachment(Attachment.PLATFORM)
                                                           .build();
        List<PublicKeyCredentialDescriptor> excludeCredentials =
                new ArrayList<PublicKeyCredentialDescriptor>();
        for (int i = 0; i < excludedCredentialIds.length; i++) {
            excludeCredentials.add(
                    new PublicKeyCredentialDescriptor(PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            excludedCredentialIds[i], new ArrayList<Transport>()));
        }
        byte[] dummy = new byte[32];
        PublicKeyCredentialCreationOptions credentialCreationOptions =
                new PublicKeyCredentialCreationOptions.Builder()
                        .setRp(new PublicKeyCredentialRpEntity(rpId, "", ""))
                        .setUser(new PublicKeyCredentialUserEntity(userId, "", null, ""))
                        .setChallenge(challenge)
                        .setParameters(parameters)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .setExcludeList(excludeCredentials)
                        .setAuthenticatorSelection(selection)
                        .setAttestationConveyancePreference(AttestationConveyancePreference.NONE)
                        .build();
        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setOrigin(Uri.parse(origin))
                        .build();
        Task<PendingIntent> result = client.getRegisterPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pedingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pedingIntent.getIntentSender(),
                              REGISTER_REQUEST_CODE,
                              null, // fillInIntent,
                              0, // flagsMask,
                              0, // flagsValue,
                              0, // extraFlags,
                              Bundle.EMPTY);
                  } catch (IntentSender.SendIntentException e) {
                      Log.e(TAG, "intent failure");
                  }
              }).addOnFailureListener(e -> { Log.e(TAG, "intent failure" + e); });

        Log.i(TAG, "op done");
    }

    public void getAssertion(
            String origin, String rpId, byte[] challenge, byte[][] allowedCredentialIds) {
        // TODO: handle concurrent requests
        Fido2PrivilegedApiClient client = Fido.getFido2PrivilegedApiClient(mContext);
        if (client == null) {
            Log.i(TAG, "getFido2PrivilegedApiClient failed");
            return;
        }
        Log.i(TAG, "have fido client");

        List<PublicKeyCredentialDescriptor> allowCredentials =
                new ArrayList<PublicKeyCredentialDescriptor>();
        ArrayList<Transport> transports = new ArrayList<Transport>();
        transports.add(Transport.INTERNAL);
        for (int i = 0; i < allowedCredentialIds.length; i++) {
            allowCredentials.add(
                    new PublicKeyCredentialDescriptor(PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            allowedCredentialIds[i], transports));
        }

        PublicKeyCredentialRequestOptions credentialRequestOptions =
                new PublicKeyCredentialRequestOptions.Builder()
                        .setAllowList(allowCredentials)
                        .setChallenge(challenge)
                        .setRpId(rpId)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .build();

        BrowserPublicKeyCredentialRequestOptions browserRequestOptions =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(credentialRequestOptions)
                        .setOrigin(Uri.parse(origin))
                        .build();

        Task<PendingIntent> result = client.getSignPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pedingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pedingIntent.getIntentSender(),
                              SIGN_REQUEST_CODE,
                              null, // fillInIntent,
                              0, // flagsMask,
                              0, // flagsValue,
                              0, // extraFlags,
                              Bundle.EMPTY);
                  } catch (IntentSender.SendIntentException e) {
                      Log.e(TAG, "intent failure");
                  }
              }).addOnFailureListener(e -> { Log.e(TAG, "intent failure" + e); });

        Log.i(TAG, "op done");
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(TAG, "onActivityResult " + requestCode + " " + resultCode);

        Result result = Result.OTHER;
        switch (requestCode) {
            case REGISTER_REQUEST_CODE:
                if (onRegisterResponse(resultCode, data)) {
                    result = Result.REGISTER_OK;
                } else {
                    result = Result.REGISTER_ERROR;
                }
                break;
            case SIGN_REQUEST_CODE:
                if (onSignResponse(resultCode, data)) {
                    result = Result.SIGN_OK;
                } else {
                    result = Result.SIGN_ERROR;
                }
                break;
            default:
                Log.i(TAG, "invalid requestCode: " + requestCode);
                assert (false);
        }
        mCallback.onAuthenticatorResult(result);
    }

    public boolean onRegisterResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code" + resultCode);
            onAuthenticatorAssertionResponse(CTAP2_ERR_OPERATION_DENIED, null, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            Log.e(TAG, "error extra");
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            // TODO: figure out translation of the remaining codes
            int ctap_status;
            switch (error.getErrorCode()) {
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            onAuthenticatorAttestationResponse(CTAP2_ERR_OTHER, null, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            onAuthenticatorAttestationResponse(CTAP2_ERR_OTHER, null, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAttestationResponse response =
                AuthenticatorAttestationResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        onAuthenticatorAttestationResponse(
                CTAP2_OK, response.getClientDataJSON(), response.getAttestationObject());
        return true;
    }

    public boolean onSignResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code" + resultCode);
            onAuthenticatorAssertionResponse(CTAP2_ERR_OPERATION_DENIED, null, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            Log.e(TAG, "error extra");
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            // TODO: figure out translation of the remaining codes
            int ctap_status;
            switch (error.getErrorCode()) {
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            onAuthenticatorAssertionResponse(ctap_status, null, null, null, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            onAuthenticatorAssertionResponse(CTAP2_ERR_OTHER, null, null, null, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAssertionResponse response =
                AuthenticatorAssertionResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        onAuthenticatorAssertionResponse(CTAP2_OK, response.getClientDataJSON(),
                response.getKeyHandle(), response.getAuthenticatorData(), response.getSignature());
        return true;
    }

    private void onAuthenticatorAttestationResponse(
            int ctapStatus, byte[] clientDataJSON, byte[] attestationObject) {
        mTaskRunner.postTask(
                ()
                        -> CableAuthenticatorJni.get().onAuthenticatorAttestationResponse(
                                ctapStatus, clientDataJSON, attestationObject));
    }

    private void onAuthenticatorAssertionResponse(int ctapStatus, byte[] clientDataJSON,
            byte[] credentialID, byte[] authenticatorData, byte[] signature) {
        mTaskRunner.postTask(
                ()
                        -> CableAuthenticatorJni.get().onAuthenticatorAssertionResponse(ctapStatus,
                                clientDataJSON, credentialID, authenticatorData, signature));
    }

    // Calls from UI.

    /**
     * Called to indicate that a QR code was scanned by the user.
     *
     * @param value contents of the QR code, which will be a valid caBLE
     *              URL, i.e. "fido://c1/"...
     */
    public void onQRCode(String value) {
        mTaskRunner.postTask(() -> { CableAuthenticatorJni.get().onQRScanned(value); });
    }

    public void close() {
        mBleHandler.close();
        mTaskRunner.postTask(() -> { CableAuthenticatorJni.get().stop(); });
    }

    @NativeMethods
    interface Natives {
        /**
         * Called to alert the C++ code to a new instance. The C++ code calls back into this object
         * to send data.
         */
        void start(CableAuthenticator cableAuthenticator, byte[] stateBytes);
        void stop();
        /**
         * Called when a QR code has been scanned.
         *
         * @param value contents of the QR code, which will be a valid caBLE
         *              URL, i.e. "fido://c1/"...
         */
        void onQRScanned(String value);
        /**
         * Called to alert the C++ code that a GATT client wrote data.
         */
        byte[][] onBLEWrite(long client, int mtu, byte[] data);
        /**
         * Called to alert native code of a response to a makeCredential request.
         */
        void onAuthenticatorAttestationResponse(
                int ctapStatus, byte[] clientDataJSON, byte[] attestationObject);
        /**
         * Called to alert native code of a response to a getAssertion request.
         */
        void onAuthenticatorAssertionResponse(int ctapStatus, byte[] clientDataJSON,
                byte[] credentialID, byte[] authenticatorData, byte[] signature);
    }
}
