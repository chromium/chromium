// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.app.Activity;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.hardware.usb.UsbAccessory;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;

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

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;

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
    private static final int CTAP2_ERR_CREDENTIAL_EXCLUDED = 0x19;
    private static final int CTAP2_ERR_OPERATION_DENIED = 0x27;
    private static final int CTAP2_ERR_UNSUPPORTED_OPTION = 0x2D;
    private static final int CTAP2_ERR_NO_CREDENTIALS = 0x2E;
    private static final int CTAP2_ERR_OTHER = 0x7F;

    // sOwnBluetooth is true if this class owns the fact that Bluetooth is enabled and needs to
    // disable it once complete.
    private static boolean sOwnBluetooth;
    // sInstanceCount is the number of instances of this class that have been created and not
    // closed.
    private static int sInstanceCount;

    private final Context mContext;
    private final CableAuthenticatorUI mUi;
    private final SingleThreadTaskRunner mTaskRunner;
    // mFCMEvent contains the serialized event data that was stored in the notification's
    // PendingIntent.
    private final byte[] mFCMEvent;
    // mServerLinkData contains the information passed from GMS Core in the event that
    // this is a SERVER_LINK connection.
    private final byte[] mServerLinkData;

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

    public CableAuthenticator(Context context, CableAuthenticatorUI ui, long networkContext,
            long registration, byte[] secret, boolean isFcmNotification, UsbAccessory accessory,
            byte[] serverLink, byte[] fcmEvent) {
        sInstanceCount++;

        mContext = context;
        mUi = ui;
        mFCMEvent = fcmEvent;
        mServerLinkData = serverLink;

        // networkContext can only be used from the UI thread, therefore all
        // short-lived work is done on that thread.
        mTaskRunner = PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.USER_VISIBLE);
        assert mTaskRunner.belongsToCurrentThread();

        CableAuthenticatorJni.get().setup(registration, networkContext, secret);

        if (accessory != null) {
            // USB mode can start immediately.
            mHandle = CableAuthenticatorJni.get().startUSB(
                    this, new USBHandler(context, mTaskRunner, accessory));
        }

        // Otherwise wait for |onQRCode| or |onBluetoothReady|.
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
    public void makeCredential(String rpId, byte[] clientDataHash, byte[] userId, int[] algorithms,
            byte[][] excludedCredentialIds, boolean residentKeyRequired) {
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
                        // This is unused because we override it with
                        // |setClientDataHash|, below. But a value must be set
                        // to prevent this Builder from throwing an exception.
                        .setChallenge(clientDataHash)
                        .setParameters(parameters)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .setExcludeList(excludeCredentials)
                        .setAuthenticatorSelection(selection)
                        .setAttestationConveyancePreference(AttestationConveyancePreference.NONE)
                        .build();
        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setClientDataHash(clientDataHash)
                        .setOrigin(Uri.parse("https://" + rpId))
                        .build();
        Task<PendingIntent> result = client.getRegisterPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pendingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pendingIntent.getIntentSender(),
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

    @CalledByNative
    public void getAssertion(String rpId, byte[] clientDataHash, byte[][] allowedCredentialIds) {
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
                        // This is unused because we override it with
                        // |setClientDataHash|, below. But a value must be set
                        // to prevent this Builder from throwing an exception.
                        .setChallenge(clientDataHash)
                        .setRpId(rpId)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .build();

        BrowserPublicKeyCredentialRequestOptions browserRequestOptions =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(credentialRequestOptions)
                        .setClientDataHash(clientDataHash)
                        .setOrigin(Uri.parse("https://" + rpId))
                        .build();

        Task<PendingIntent> result = client.getSignPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pendingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pendingIntent.getIntentSender(),
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

    /**
     * Called from native code when a network-based operation has completed.
     *
     * @param ok true if the transaction completed successfully. Otherwise it
     *           indicates some form of error that could include tunnel server
     *           errors, handshake failures, etc.
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    @CalledByNative
    public void onComplete(boolean ok, int errorCode) {
        assert mTaskRunner.belongsToCurrentThread();
        mUi.onComplete(ok, errorCode);
    }

    void onActivityStop() {
        CableAuthenticatorJni.get().onActivityStop(mHandle);
    }

    void onActivityResult(int requestCode, int resultCode, Intent data) {
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
        mUi.onAuthenticatorResult(result);
    }

    private boolean onRegisterResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code " + resultCode);
            onAuthenticatorAssertionResponse(CTAP2_ERR_OPERATION_DENIED, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            int ctap_status;
            switch (error.getErrorCode()) {
                case INVALID_STATE_ERR:
                    // Assumed to be caused by a matching excluded credential.
                    // (It's possible to match the error string to be sure,
                    // but that's fragile.)
                    ctap_status = CTAP2_ERR_CREDENTIAL_EXCLUDED;
                    break;
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            onAuthenticatorAttestationResponse(CTAP2_ERR_OTHER, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            onAuthenticatorAttestationResponse(CTAP2_ERR_OTHER, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAttestationResponse response =
                AuthenticatorAttestationResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        onAuthenticatorAttestationResponse(CTAP2_OK, response.getAttestationObject());
        return true;
    }

    private boolean onSignResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code " + resultCode);
            onAuthenticatorAssertionResponse(CTAP2_ERR_OPERATION_DENIED, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            int ctap_status;
            switch (error.getErrorCode()) {
                case INVALID_STATE_ERR:
                    // Assumed to be because none of the credentials were
                    // recognised. (It's possible to match the error string to
                    // be sure, but that's fragile.)
                    ctap_status = CTAP2_ERR_NO_CREDENTIALS;
                    break;
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            onAuthenticatorAssertionResponse(ctap_status, null, null, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            onAuthenticatorAssertionResponse(CTAP2_ERR_OTHER, null, null, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAssertionResponse response =
                AuthenticatorAssertionResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        onAuthenticatorAssertionResponse(CTAP2_OK, response.getKeyHandle(),
                response.getAuthenticatorData(), response.getSignature());
        return true;
    }

    private void onAuthenticatorAttestationResponse(int ctapStatus, byte[] attestationObject) {
        mTaskRunner.postTask(
                ()
                        -> CableAuthenticatorJni.get().onAuthenticatorAttestationResponse(
                                ctapStatus, attestationObject));
    }

    private void onAuthenticatorAssertionResponse(
            int ctapStatus, byte[] credentialID, byte[] authenticatorData, byte[] signature) {
        mTaskRunner.postTask(
                ()
                        -> CableAuthenticatorJni.get().onAuthenticatorAssertionResponse(
                                ctapStatus, credentialID, authenticatorData, signature));
    }

    // Calls from UI.

    /**
     * Called to indicate that a QR code was scanned by the user.
     *
     * @param value contents of the QR code, which will be a valid caBLE
     *              URL, i.e. "fido://"...
     */
    void onQRCode(String value, boolean link) {
        assert mTaskRunner.belongsToCurrentThread();
        mHandle = CableAuthenticatorJni.get().startQR(this, getName(), value, link);
        // TODO: show the user an error if that returned zero.
        // that indicates that the QR code was invalid.
    }

    /**
     * Called to indicate that Bluetooth is now enabled and a cloud message can be processed.
     *
     * @param needToDisable true if BLE needs to be disabled afterwards
     */
    void onBluetoothReady(boolean needToDisable) {
        assert mTaskRunner.belongsToCurrentThread();
        sOwnBluetooth |= needToDisable;
        if (mServerLinkData != null) {
            mHandle = CableAuthenticatorJni.get().startServerLink(this, mServerLinkData);
        } else {
            mHandle = CableAuthenticatorJni.get().startCloudMessage(this, mFCMEvent);
        }
    }

    void unlinkAllDevices() {
        Log.i(TAG, "Unlinking devices");
        CableAuthenticatorJni.get().unlink();
    }

    void close() {
        assert mTaskRunner.belongsToCurrentThread();
        CableAuthenticatorJni.get().stop(mHandle);

        // If Bluetooth was enabled by CableAuthenticatorUI then |sOwnBluetooth| will be true.
        // However, if another instance has already been created (because the user pressed another
        // notification while this was still outstanding) then don't disable it yet.
        sInstanceCount--;
        if (sOwnBluetooth) {
            if (sInstanceCount == 0) {
                if (BuildInfo.isAtLeastS()) {
                    // It's not possible to disable Bluetooth on Android 12 without another
                    // permission prompt.
                    Log.i(TAG, "not trying to disable Bluetooth on Android 12");
                } else {
                    Log.i(TAG, "disabling Bluetooth");
                    BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                    adapter.disable();
                }

                sOwnBluetooth = false;
            } else {
                Log.i(TAG, "not disabling Bluetooth yet because other instances exist");
            }
        }
    }

    String getName() {
        final String name = Settings.Global.getString(
                mContext.getContentResolver(), Settings.Global.DEVICE_NAME);
        if (name != null && name.length() > 0) {
            return name;
        }
        return Build.MANUFACTURER + " " + Build.MODEL;
    }

    /**
     * validateServerLinkData returns zero if |serverLink| is a valid argument for
     * |startServerLink| or else an error value from cablev2::authenticator::Platform::Error.
     */
    static int validateServerLinkData(byte[] serverLinkData) {
        return CableAuthenticatorJni.get().validateServerLinkData(serverLinkData);
    }

    @NativeMethods
    interface Natives {
        /**
         * setup is called before any other functions in order for the native code to perform
         * one-time setup operations. It may be called several times, but subsequent calls are
         * ignored.
         */
        void setup(long registration, long networkContext, byte[] secret);

        /**
         * Called to instruct the C++ code to start a new transaction using |usbDevice|. Returns an
         * opaque value that can be passed to |stop| to cancel this transaction.
         */
        long startUSB(CableAuthenticator cableAuthenticator, USBHandler usbDevice);

        /**
         * Called to instruct the C++ code to start a new transaction based on the contents of a QR
         * code. The given name will be transmitted to the peer in order to identify this device, it
         * should be human-meaningful. The qrUrl must be a caBLE URL, i.e. starting with
         * "fido://c1/". Returns an opaque value that can be passed to |stop| to cancel this
         * transaction.
         */
        long startQR(CableAuthenticator cableAuthenticator, String authenticatorName, String qrUrl,
                boolean link);

        /**
         * Called to instruct the C++ code to start a new transaction based on the given link
         * information which has been provided by the server. Returns an opaque value that can be
         * passed to |stop| to cancel this transaction.
         */
        long startServerLink(CableAuthenticator cableAuthenticator, byte[] serverLinkData);

        /**
         * Called when a GCM message is received and the user has tapped on the resulting
         * notification. fcmEvent contains a serialized event, as created by
         * |webauthn::authenticator::Registration::Event::Serialize|.
         */
        long startCloudMessage(CableAuthenticator cableAuthenticator, byte[] fcmEvent);

        /**
         * unlink causes the linking FCM token to be rotated. This prevents all previously linked
         * devices from being able to contact this device in the future -- they'll have to go via
         * the QR-scanning path again.
         */
        void unlink();

        /**
         * Called to alert the C++ code to stop any ongoing transactions. Takes an opaque handle
         * value that was returned by one of the |start*| functions.
         */
        void stop(long handle);

        /**
         * validateServerLinkData returns zero if |serverLink| is a valid argument for
         * |startServerLink| or else an error value from cablev2::authenticator::Platform::Error.
         */
        int validateServerLinkData(byte[] serverLinkData);

        /**
         * onActivityStop is called when onStop() is called on the Activity. This is done
         * in order to record events because we want to know when users are abandoning
         * the process.
         */
        void onActivityStop(long handle);

        /**
         * Called to alert native code of a response to a makeCredential request.
         */
        void onAuthenticatorAttestationResponse(int ctapStatus, byte[] attestationObject);

        /**
         * Called to alert native code of a response to a getAssertion request.
         */
        void onAuthenticatorAssertionResponse(
                int ctapStatus, byte[] credentialID, byte[] authenticatorData, byte[] signature);
    }
}
