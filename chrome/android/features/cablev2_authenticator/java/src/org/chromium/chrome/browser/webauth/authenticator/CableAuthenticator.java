// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.SharedPreferences;
import android.hardware.usb.UsbAccessory;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

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

import org.chromium.base.ContextUtils;
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
    private static final String NOTIFICATION_CHANNEL_ID =
            "chrome.android.features.cablev2_authenticator";
    // ID is used when Android APIs demand a process-wide unique ID. This number
    // is a random int.
    private static final int ID = 424386536;

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
    private final SingleThreadTaskRunner mTaskRunner;

    public enum Result {
        REGISTER_OK,
        REGISTER_ERROR,
        SIGN_OK,
        SIGN_ERROR,
        OTHER,
    }

    public CableAuthenticator(Context context, CableAuthenticatorUI ui, long networkContext,
            long registration, String activityClassName, boolean isFcmNotification,
            UsbAccessory accessory, byte[] serverLink) {
        mContext = context;
        mUi = ui;

        // networkContext can only be used from the UI thread, therefore all
        // short-lived work is done on that thread.
        mTaskRunner = PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.USER_VISIBLE);
        assert mTaskRunner.belongsToCurrentThread();

        setup(registration, activityClassName, networkContext);

        if (accessory != null) {
            // USB mode can start immediately.
            CableAuthenticatorJni.get().startUSB(
                    this, new USBHandler(context, mTaskRunner, accessory));
        }

        if (isFcmNotification) {
            // The user tapped a notification that resulted from an FCM message.
            CableAuthenticatorJni.get().onInteractionReady(this);
        }

        if (serverLink != null) {
            CableAuthenticatorJni.get().startServerLink(this, serverLink);
        }

        // Otherwise wait for a QR scan.
    }

    // setup initialises the native code. This is idempotent.
    private static void setup(long registration, String activityClassName, long networkContext) {
        // SharedPreferences in Chromium is loaded and cached at startup, and
        // applying changes is done asynchronously. Thus it's ok to do here, on
        // the UI thread.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        byte[] stateBytes;
        try {
            stateBytes = Base64.decode(prefs.getString(STATE_VALUE_NAME, ""), Base64.DEFAULT);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Ignoring corrupt state");
            stateBytes = new byte[0];
        }

        byte[] newStateBytes = CableAuthenticatorJni.get().setup(
                registration, activityClassName, networkContext, stateBytes);
        if (newStateBytes.length > 0) {
            Log.i(TAG, "Writing updated state");
            prefs.edit()
                    .putString(STATE_VALUE_NAME,
                            Base64.encodeToString(
                                    newStateBytes, Base64.NO_WRAP | Base64.NO_PADDING))
                    .apply();
        }
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
     */
    @CalledByNative
    public void onComplete() {
        assert mTaskRunner.belongsToCurrentThread();
        mUi.onComplete();
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

    private boolean onSignResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code " + resultCode);
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
     *              URL, i.e. "fido://"...
     */
    void onQRCode(String value, boolean link) {
        assert mTaskRunner.belongsToCurrentThread();
        CableAuthenticatorJni.get().startQR(this, getName(), value, link);
        // TODO: show the user an error if that returned false.
        // that indicates that the QR code was invalid.
    }

    void unlinkAllDevices() {
        Log.i(TAG, "Unlinking devices");
        byte[] newStateBytes = CableAuthenticatorJni.get().unlink();
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit()
                .putString(STATE_VALUE_NAME,
                        Base64.encodeToString(newStateBytes, Base64.NO_WRAP | Base64.NO_PADDING))
                .apply();
    }

    void close() {
        assert mTaskRunner.belongsToCurrentThread();
        CableAuthenticatorJni.get().stop();
    }

    static String getName() {
        final String name = BluetoothAdapter.getDefaultAdapter().getName();
        if (name != null && name.length() > 0) {
            return name;
        }
        return Build.MANUFACTURER + " " + Build.MODEL;
    }

    /**
     * onCloudMessage is called by {@link CableAuthenticatorUI} when a GCM message is received.
     */
    static void onCloudMessage(long event, long systemNetworkContext, long registration,
            String activityClassName, boolean needToDisableBluetooth) {
        setup(registration, activityClassName, systemNetworkContext);
        CableAuthenticatorJni.get().onCloudMessage(event, needToDisableBluetooth);
    }

    /**
     * showNotification is called by the C++ code to show an Android
     * notification. When pressed, the notification will activity the given
     * Activity and Fragment.
     */
    // TODO: localize
    @SuppressLint("SetTextI18n")
    @CalledByNative
    public static void showNotification(String activityClassName) {
        Context context = ContextUtils.getApplicationContext();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Register a channel for this notification. Registering the same
            // channel twice is harmless.
            CharSequence name = "Security key activations";
            String description =
                    "Notifications that appear when you attempt to log in on another device";
            int importance = NotificationManager.IMPORTANCE_HIGH;
            NotificationChannel channel =
                    new NotificationChannel(NOTIFICATION_CHANNEL_ID, name, importance);
            channel.setDescription(description);
            NotificationManager notificationManager =
                    context.getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }

        Intent intent;
        try {
            intent = new Intent(context, Class.forName(activityClassName));
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Failed to find class " + activityClassName);
            return;
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        Bundle bundle = new Bundle();
        bundle.putBoolean("org.chromium.chrome.modules.cablev2_authenticator.FCM", true);
        intent.putExtra("show_fragment_args", bundle);
        PendingIntent pendingIntent = PendingIntent.getActivity(context, ID, intent, 0);

        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(context, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(android.R.drawable.stat_sys_download_done)
                        .setContentTitle("Press to log in")
                        .setContentText("A paired device is attempting to log in")
                        .setPriority(NotificationCompat.PRIORITY_HIGH)
                        .setAutoCancel(true)
                        .setContentIntent(pendingIntent)
                        .setVisibility(NotificationCompat.VISIBILITY_PUBLIC);

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.notify(NOTIFICATION_CHANNEL_ID, ID, builder.build());
    }

    @CalledByNative
    public static void dropNotification() {
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.cancel(NOTIFICATION_CHANNEL_ID, ID);
    }

    @CalledByNative
    public static void disableBluetooth() {
        Log.i(TAG, "Operation complete. Disabling Bluetooth.");
        BluetoothAdapter.getDefaultAdapter().disable();
    }

    @NativeMethods
    interface Natives {
        /**
         * setup is called before any other functions in order for the native code to perform
         * one-time setup operations. It may be called several times, but subsequent calls are
         * ignored. It returns an empty byte array if the given state is valid, or the new contents
         * of the persisted state otherwise.
         */
        byte[] setup(long registration, String activityClassName, long networkContext,
                byte[] stateBytes);

        /**
         * Called to instruct the C++ code to start a new transaction using |usbDevice|.
         */
        void startUSB(CableAuthenticator cableAuthenticator, USBHandler usbDevice);

        /**
         * Called to instruct the C++ code to start a new transaction based on the contents of a QR
         * code. The given name will be transmitted to the peer in order to identify this device, it
         * should be human-meaningful. The qrUrl must be a caBLE URL, i.e. starting with
         * "fido://c1/"
         */
        boolean startQR(CableAuthenticator cableAuthenticator, String authenticatorName,
                String qrUrl, boolean link);

        /**
         * Called to instruct the C++ code to start a new transaction based on the given link
         * information which has been provided by the server.
         */
        boolean startServerLink(CableAuthenticator cableAuthenticator, byte[] serverLinkData);

        /**
         * unlink causes the root secret to be rotated and the FCM token to be rotated. This
         * prevents all previously linked devices from being able to contact this device in the
         * future -- they'll have to go via the QR-scanning path again. It returns the updated state
         * which must be persisted.
         */
        byte[] unlink();

        /**
         * Called after the notification created by {@link showNotification} has been pressed and
         * the {@link CableAuthenticatorUI} Fragment is now in the foreground for showing UI.
         */
        void onInteractionReady(CableAuthenticator cableAuthenticator);

        /**
         * Called to alert the C++ code to stop any ongoing transactions.
         */
        void stop();

        /**
         * Called when a GCM message is received. The |event| argument is a pointer to a
         * |device::cablev2::authenticator::Registration::Event| object that the native code takes
         * ownership of. |needToDisableBluetooth| is true if Bluetooth was enabled for the purposes
         * of processing this event and thus |disableBluetooth| should be called once complete.
         */
        void onCloudMessage(long event, boolean needToDisableBluetooth);

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
