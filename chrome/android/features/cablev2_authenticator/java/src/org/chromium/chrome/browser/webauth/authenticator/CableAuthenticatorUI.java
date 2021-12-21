// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.KeyguardManager;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * A fragment that provides a UI for various caBLE v2 actions.
 */
public class CableAuthenticatorUI extends Fragment implements OnClickListener {
    private static final String TAG = "CableAuthenticatorUI";

    // ENABLE_BLUETOOTH_REQUEST_CODE is a random int used to identify responses
    // to a request to enable Bluetooth. (Request codes can only be 16-bit.)
    private static final int ENABLE_BLUETOOTH_REQUEST_CODE = 64907;

    // BLE_SCREEN_DELAY_SECS is the number of seconds that the screen for BLE
    // enabling will show before the request to actually enable BLE (which
    // causes Android to draw on top of it) is made.
    private static final int BLE_SCREEN_DELAY_SECS = 2;

    // USB_PROMPT_TIMEOUT_SECS is the number of seconds the spinner will show
    // for before being replaced with a prompt to connect via USB cable.
    private static final int USB_PROMPT_TIMEOUT_SECS = 20;

    private static final String FCM_EXTRA = "org.chromium.chrome.modules.cablev2_authenticator.FCM";
    private static final String EVENT_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.EVENT";
    private static final String NETWORK_CONTEXT_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String SECRET_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";
    private static final String METRICS_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.MetricsEnabled";
    private static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";
    private static final String QR_EXTRA = "org.chromium.chrome.browser.webauth.authenticator.QR";

    // These entries duplicate some of the enum values from
    // device::cablev2::authenticator::Platform::Error. They must be kept in
    // sync with the C++ side because C++ communicates these values to this
    // code.
    private static final int ERROR_NONE = 0;
    private static final int ERROR_UNEXPECTED_EOF = 100;
    private static final int ERROR_NO_SCREENLOCK = 110;
    private static final int ERROR_NO_BLUETOOTH_PERMISSION = 111;

    private enum Mode {
        QR, // QR code scanned by external app.
        FCM, // Triggered by user selecting notification; handshake already running.
        USB, // Triggered by connecting via USB.
        SERVER_LINK, // Triggered by GMSCore forwarding from GAIA.
    }
    private Mode mMode;

    // Save for ERROR, states always move from one to the next. There are no
    // cycles. Different modes start in different states, e.g. only QR mode
    // starts with QR_CONFIRM.
    private enum State {
        QR_CONFIRM,
        ENABLE_BLUETOOTH,
        ENABLE_BLUETOOTH_REQUESTED,
        BLUETOOTH_PERMISSION,
        BLUETOOTH_PERMISSION_REQUESTED,
        RUNNING,
        ERROR,
    }
    private State mState;

    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;
    private TextView mStatusText;
    private View mErrorView;
    private View mErrorCloseButton;
    private View mErrorSettingsButton;
    private View mSpinnerView;
    private View mBLEEnableView;
    private View mQRAllowButton;
    private View mQRRejectButton;

    // mErrorCode contains a value of the authenticator::Platform::Error
    // enumeration when |mState| is |ERROR|.
    private int mErrorCode;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        // This code should not be reachable on older Android versions.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;

        super.onCreate(savedInstanceState);
        final Context context = getContext();

        Bundle arguments = getArguments();
        final UsbAccessory accessory =
                (UsbAccessory) arguments.getParcelable(UsbManager.EXTRA_ACCESSORY);
        final byte[] serverLink = arguments.getByteArray(SERVER_LINK_EXTRA);
        final byte[] fcmEvent = arguments.getByteArray(EVENT_EXTRA);
        final Uri qrFromArguments = (Uri) arguments.getParcelable(QR_EXTRA);
        final String qrURI = qrFromArguments == null ? null : qrFromArguments.toString();
        if (accessory != null) {
            mMode = Mode.USB;
        } else if (arguments.getBoolean(FCM_EXTRA)) {
            mMode = Mode.FCM;
        } else if (serverLink != null) {
            mErrorCode = CableAuthenticator.validateServerLinkData(serverLink);
            if (mErrorCode != ERROR_NONE) {
                mState = State.ERROR;
                return;
            }

            mMode = Mode.SERVER_LINK;
        } else if (qrURI != null) {
            mErrorCode = CableAuthenticator.validateQRURI(qrURI);
            if (mErrorCode != ERROR_NONE) {
                mState = State.ERROR;
                return;
            }

            mMode = Mode.QR;
        } else {
            assert false;
            getActivity().finish();
        }

        // GMSCore will immediately fail all requests if a screenlock isn't
        // configured, except for server-link because PaaSK is specific.
        // Outside of server-link, the device shouldn't have advertised itself
        // via Sync, but it's possible for a request to come in soon after a
        // screen lock was removed.
        if (mMode != Mode.SERVER_LINK && !hasScreenLockConfigured(context)) {
            mState = State.ERROR;
            mErrorCode = ERROR_NO_SCREENLOCK;
        }

        if (mState == State.ERROR) {
            Log.i(TAG, "Preconditions failed");
            return;
        }

        Log.i(TAG, "Starting in mode " + mMode.toString());

        final long networkContext = arguments.getLong(NETWORK_CONTEXT_EXTRA);
        final long registration = arguments.getLong(REGISTRATION_EXTRA);
        final byte[] secret = arguments.getByteArray(SECRET_EXTRA);
        final boolean metricsEnabled = arguments.getBoolean(METRICS_EXTRA);

        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
        mAuthenticator = new CableAuthenticator(getContext(), this, networkContext, registration,
                secret, mMode == Mode.FCM, accessory, serverLink, fcmEvent, qrURI, metricsEnabled);

        switch (mMode) {
            case USB:
                // USB mode doesn't require Bluetooth.
                mState = State.RUNNING;
                break;

            case SERVER_LINK:
            case FCM:
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                if (!adapter.isEnabled()) {
                    // A screen for enabling Bluetooth will be shown and
                    // onResume will trigger the permission request.
                    mState = State.ENABLE_BLUETOOTH;
                    return;
                }
                mState = State.BLUETOOTH_PERMISSION;
                break;

            case QR:
                // QR mode displays a confirmation UI first.
                mState = State.QR_CONFIRM;
                break;
        }

        // Reference these strings to avoid an error about them being unused.
        // They're checked in for translation ahead of the code that uses them.
        // TODO(agl): remove this.
        getResources().getString(R.string.cablev2_linked_devices);
        getResources().getString(R.string.cablev2_linked_devices_body);
        getResources().getString(R.string.cablev2_paask_body);
        getResources().getString(R.string.cablev2_paask_title);
        getResources().getString(R.string.cablev2_unlink_button);
        getResources().getString(R.string.cablev2_unlink_confirmation);
        getResources().getString(R.string.cablev2_your_devices);
        getResources().getString(R.string.cablev2_your_devices_body);
    }

    // This class should not be reachable on Android versions < N (API level 24).
    @TargetApi(24)
    private static boolean hasScreenLockConfigured(Context context) {
        KeyguardManager km = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        return km.isDeviceSecure();
    }

    private View createSpinnerScreen(LayoutInflater inflater, ViewGroup container) {
        View v = inflater.inflate(R.layout.cablev2_spinner, container, false);
        mStatusText = v.findViewById(R.id.status_text);

        final AnimatedVectorDrawableCompat anim = AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.circle_loader_animation);
        // There is no way to make an animation loop. Instead it must be
        // manually started each time it completes.
        anim.registerAnimationCallback(new Animatable2Compat.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                if (drawable != null) {
                    anim.start();
                }
            }
        });
        ((ImageView) v.findViewById(R.id.spinner)).setImageDrawable(anim);
        anim.start();

        return v;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        getActivity().setTitle(R.string.cablev2_activity_title);
        ViewGroup top = new LinearLayout(getContext());

        mErrorView = inflater.inflate(R.layout.cablev2_error, container, false);
        mSpinnerView = createSpinnerScreen(inflater, container);
        mBLEEnableView = inflater.inflate(R.layout.cablev2_ble_enable, container, false);

        View v = null;
        if (mState == State.ENABLE_BLUETOOTH) {
            v = mBLEEnableView;
        } else if (mState == State.ERROR) {
            fillOutErrorUI(mErrorCode);
            v = mErrorView;
        } else {
            switch (mMode) {
                case USB:
                    v = inflater.inflate(R.layout.cablev2_usb_attached, container, false);
                    break;

                case FCM:
                case SERVER_LINK:
                    v = mSpinnerView;
                    break;

                case QR:
                    // TODO: strings should be translated but this will be replaced during
                    // the UI process.
                    v = inflater.inflate(R.layout.cablev2_qr, container, false);

                    mQRAllowButton = v.findViewById(R.id.qr_connect);
                    mQRAllowButton.setOnClickListener(this);

                    mQRRejectButton = v.findViewById(R.id.qr_reject);
                    mQRRejectButton.setOnClickListener(this);

                    break;
            }
        }

        top.addView(v);
        return top;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mState == State.ENABLE_BLUETOOTH) {
            enableBluetooth();
        } else if (mState == State.BLUETOOTH_PERMISSION) {
            onBluetoothEnabled();
        } else if (mState == State.ERROR && mErrorCode == ERROR_NO_BLUETOOTH_PERMISSION
                && BuildInfo.isAtLeastS()) {
            // This needs to be in a different function call to use functions above API level 21.
            maybeResolveBLEPermissionError();
        }
    }

    // Called when the activity is resumed in a BLE permission error state.
    @TargetApi(31)
    private void maybeResolveBLEPermissionError() {
        if (getContext().checkSelfPermission(permission.BLUETOOTH_ADVERTISE)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }

        // The user navigated away and came back, but now we have the needed permission.
        ViewGroup top = (ViewGroup) getView();
        top.removeAllViews();
        top.addView(mSpinnerView);

        mErrorCode = 0;
        onHaveBluetoothPermission();
    }

    private void enableBluetooth() {
        assert mState == State.ENABLE_BLUETOOTH;

        mState = State.ENABLE_BLUETOOTH_REQUESTED;
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mAuthenticator != null) {
                startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
                        ENABLE_BLUETOOTH_REQUEST_CODE);
            }
        }, BLE_SCREEN_DELAY_SECS * 1000);
    }

    // Called when the Bluetooth adapter has been enabled, or was already enabled.
    private void onBluetoothEnabled() {
        // In Android 12 and above there is a new BLUETOOTH_ADVERTISE runtime permission.
        if (BuildInfo.isAtLeastS()) {
            maybeGetBluetoothPermission();
            return;
        }

        onHaveBluetoothPermission();
    }

    // Called on Android 12 or later after the Bluetooth adaptor is enabled.
    @TargetApi(31)
    private void maybeGetBluetoothPermission() {
        mState = State.BLUETOOTH_PERMISSION;
        final String advertise = permission.BLUETOOTH_ADVERTISE;

        if (getContext().checkSelfPermission(advertise) == PackageManager.PERMISSION_GRANTED) {
            onHaveBluetoothPermission();
            return;
        }

        if (shouldShowRequestPermissionRationale(advertise)) {
            // Since the user took explicit action to make a connection to their
            // computer, and there's a big "Connecting to your computer" in the
            // background, the rationale should be clear. However, this
            // function must always be called otherwise the permission will be
            // automatically refused.
        }

        // The |Fragment| method |requestPermissions| is called rather than
        // the method on |mPermissionDelegate| because the latter routes the
        // |onRequestPermissionsResult| callback to the Activity, and not
        // this fragment.
        mState = State.BLUETOOTH_PERMISSION_REQUESTED;
        requestPermissions(new String[] {advertise}, 1);
    }

    // Called once the BLUETOOTH_ADVERTISE permission has been granted, or if
    // its not needed on this version of Android.
    private void onHaveBluetoothPermission() {
        mState = State.RUNNING;
        mAuthenticator.onBluetoothReady();
    }

    /**
     * Called when the button to scan a QR code is pressed.
     */
    @Override
    public void onClick(View v) {
        if (v == mErrorCloseButton) {
            getActivity().finish();
        } else if (v == mErrorSettingsButton) {
            // Open the Settings screen for Chromium.
            Intent intent =
                    new Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intent.setData(android.net.Uri.fromParts(
                    "package", BuildInfo.getInstance().packageName, null));
            startActivity(intent);
        } else if (v == mQRAllowButton) {
            // User approved a QR transaction.

            ViewGroup top = (ViewGroup) getView();
            mAuthenticator.setQRLinking(((CheckBox) top.findViewById(R.id.qr_link)).isChecked());

            top.removeAllViews();
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (!adapter.isEnabled()) {
                mState = State.ENABLE_BLUETOOTH;
                top.addView(mBLEEnableView);
                enableBluetooth();
            } else {
                mState = State.BLUETOOTH_PERMISSION;
                top.addView(mSpinnerView);
                onBluetoothEnabled();
            }
        } else if (v == mQRRejectButton) {
            // User rejected a QR scan.
            getActivity().finish();
        }
    }

    void onStatus(int code) {
        switch (mMode) {
            case QR:
            case SERVER_LINK:
            case FCM:
                // These values must match up with the Status enum in v2_authenticator.h
                int id = -1;
                if (code == 1) {
                    if (mMode == Mode.SERVER_LINK) {
                        id = R.string.cablev2_serverlink_status_connecting;
                    } else {
                        id = R.string.cablev2_fcm_status_connecting;
                    }
                } else if (code == 2) {
                    id = R.string.cablev2_serverlink_status_connected;
                } else if (code == 3) {
                    id = R.string.cablev2_serverlink_status_processing;
                } else {
                    break;
                }

                mStatusText.setText(getResources().getString(id));
                break;

            case USB:
                // In USB mode everything should happen immediately.
                break;
        }
    }

    /**
     * Called when camera permission has been requested and the user has resolved the permission
     * request.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        final boolean granted =
                grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;

        switch (mState) {
            case BLUETOOTH_PERMISSION_REQUESTED:
                if (granted) {
                    assert permissions[0].equals(permission.BLUETOOTH_ADVERTISE);
                    onHaveBluetoothPermission();
                } else {
                    mState = State.ERROR;

                    mErrorCode = ERROR_NO_BLUETOOTH_PERMISSION;
                    fillOutErrorUI(mErrorCode);
                    ViewGroup top = (ViewGroup) getView();
                    top.removeAllViews();
                    top.addView(mErrorView);
                }
                break;

            default:
                assert false;
        }
    }

    @Override
    public void onStop() {
        super.onStop();

        if (mAuthenticator != null) {
            mAuthenticator.onActivityStop();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // Closing mAuthenticator is not done in |onStop| because Android can
        // generate spurious |onStop| calls when the activity is started from
        // a lock-screen notification.
        if (mAuthenticator != null) {
            mAuthenticator.close();
            mAuthenticator = null;
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (mAuthenticator == null) {
            return;
        }

        if (requestCode != ENABLE_BLUETOOTH_REQUEST_CODE) {
            mAuthenticator.onActivityResult(requestCode, resultCode, data);
            return;
        }

        if (resultCode != Activity.RESULT_OK) {
            getActivity().finish();
            return;
        }

        switch (mMode) {
            case QR:
            case SERVER_LINK:
            case FCM:
                ViewGroup top = (ViewGroup) getView();
                top.removeAllViews();
                top.addView(mSpinnerView);

                onBluetoothEnabled();
                break;

            default:
                assert false;
        }
    }

    void onAuthenticatorConnected() {}

    void onAuthenticatorResult(CableAuthenticator.Result result) {
        getActivity().runOnUiThread(() -> {
            int id = -1;
            switch (result) {
                case REGISTER_OK:
                    id = R.string.cablev2_registration_succeeded;
                    break;
                case REGISTER_ERROR:
                    id = R.string.cablev2_registration_failed;
                    break;
                case SIGN_OK:
                    id = R.string.cablev2_sign_in_succeeded;
                    break;
                case SIGN_ERROR:
                case OTHER:
                    id = R.string.cablev2_sign_in_failed;
                    break;
            }
            Toast.makeText(getActivity(), getResources().getString(id), Toast.LENGTH_SHORT).show();

            // Finish the Activity unless we're connected via USB. In that case
            // we continue to show a message advising the user to disconnect
            // the cable because the USB connection is in AOA mode.
            if (mMode != Mode.USB) {
                getActivity().finish();
            }
        });
    }

    /**
     * Called when a transaction has completed.
     *
     * @param ok true if the transaction completed successfully. Otherwise it
     *           indicates some form of error that could include tunnel server
     *           errors, handshake failures, etc.
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    void onComplete(boolean ok, int errorCode) {
        ThreadUtils.assertOnUiThread();

        if (ok) {
            getActivity().finish();
            return;
        }

        fillOutErrorUI(errorCode);
        ViewGroup top = (ViewGroup) getView();
        top.removeAllViews();
        top.addView(mErrorView);
    }

    /**
     * Fills out the elements of |mErrorView| for the given error code.
     *
     * @param errorCode a value from cablev2::authenticator::Platform::Error.
     */
    void fillOutErrorUI(int errorCode) {
        mErrorCloseButton = mErrorView.findViewById(R.id.error_close);
        mErrorCloseButton.setOnClickListener(this);
        mErrorSettingsButton = mErrorView.findViewById(R.id.error_settings_button);
        mErrorSettingsButton.setOnClickListener(this);

        String desc;
        boolean settingsButtonVisible = false;
        switch (errorCode) {
            case ERROR_UNEXPECTED_EOF:
                desc = getResources().getString(R.string.cablev2_error_timeout);
                break;

            case ERROR_NO_BLUETOOTH_PERMISSION:
                final String packageLabel = BuildInfo.getInstance().hostPackageLabel;
                desc = getResources().getString(
                        R.string.cablev2_error_ble_permission, packageLabel);
                settingsButtonVisible = true;
                break;

            default:
                TextView errorCodeTextView = (TextView) mErrorView.findViewById(R.id.error_code);
                errorCodeTextView.setText(
                        getResources().getString(R.string.cablev2_error_code, errorCode));

                desc = getResources().getString(R.string.cablev2_error_generic);
                break;
        }

        ((View) mErrorView.findViewById(R.id.error_settings_button))
                .setVisibility(settingsButtonVisible ? View.VISIBLE : View.INVISIBLE);

        TextView descriptionTextView = (TextView) mErrorView.findViewById(R.id.error_description);
        descriptionTextView.setText(desc);
    }
}
