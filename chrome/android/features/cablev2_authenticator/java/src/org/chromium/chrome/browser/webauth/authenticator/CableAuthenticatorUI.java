// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.KeyguardManager;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.core.content.res.ResourcesCompat;
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
 * A fragment that provides a UI for scanning caBLE v2 QR codes.
 */
public class CableAuthenticatorUI
        extends Fragment implements OnClickListener, QRScanDialog.Callback {
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
    private static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";

    // These entries duplicate some of the enum values from
    // device::cablev2::authenticator::Platform::Error. They must be kept in
    // sync with the C++ side because C++ communicates these values to this
    // code.
    private static final int ERROR_NONE = 0;
    private static final int ERROR_UNEXPECTED_EOF = 100;
    private static final int ERROR_NO_SCREENLOCK = 110;
    private static final int ERROR_NO_BLUETOOTH_PERMISSION = 111;

    private enum Mode {
        QR, // Triggered from Settings; can scan QR code to start handshake.
        FCM, // Triggered by user selecting notification; handshake already running.
        USB, // Triggered by connecting via USB.
        SERVER_LINK, // Triggered by GMSCore forwarding from GAIA.
    }
    private Mode mMode;

    // Save for ERROR, states always move from one to the next. There are no
    // cycles. Different modes start in different states, e.g. only QR mode
    // starts with SETTINGS_UI.
    private enum State {
        SETTINGS_UI,
        CAMERA_PERMISSION,
        QR_SCANNING,
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
    private LinearLayout mQRButton;
    private LinearLayout mUnlinkButton;
    private ImageView mHeader;
    private TextView mStatusText;
    private View mErrorView;
    private View mErrorCloseButton;
    private View mSpinnerView;

    // mErrorCode contains a value of the authenticator::Platform::Error
    // enumeration when |mState| is |ERROR|.
    private int mErrorCode;

    // The following two members store a pending QR-scan result while Bluetooth
    // is enabled.
    private String mPendingQRCode;
    private boolean mPendingShouldLink;

    // This is true if we enabled Bluetooth and thus need to disable it once
    // complete. This is only set while a Bluetooth permission prompt is
    // outstanding, otherwise the value is passed around as a parameter.
    // (A permission prompt is only shown on Android >=12, and on Android >=12
    // it's not possible to disable Bluetooth. But we continue to accurately
    // track this boolean in case that changes.)
    private boolean mNeedToDisableBluetooth;

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
        } else {
            mMode = Mode.QR;
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

        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
        mAuthenticator = new CableAuthenticator(getContext(), this, networkContext, registration,
                secret, mMode == Mode.FCM, accessory, serverLink, fcmEvent);

        switch (mMode) {
            case USB:
                // USB mode doesn't require Bluetooth.
                mState = State.RUNNING;
                break;

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

            case SERVER_LINK:
                // GMS Core should have taken care of enabling Bluetooth.
                mState = State.BLUETOOTH_PERMISSION;
                break;

            case QR:
                // QR mode displays a UI first.
                mState = State.SETTINGS_UI;
                break;
        }
    }

    // This class should not be reachable on Android versions < N (API level 24).
    @TargetApi(24)
    private static boolean hasScreenLockConfigured(Context context) {
        KeyguardManager km = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        return km.isDeviceSecure();
    }

    private View createSpinnerScreen(LayoutInflater inflater, ViewGroup container) {
        View v = inflater.inflate(R.layout.cablev2_serverlink, container, false);
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

        // Inflate the error view in case it's needed later.
        mErrorView = inflater.inflate(R.layout.cablev2_error, container, false);

        View v = null;

        if (mState == State.ENABLE_BLUETOOTH) {
            v = inflater.inflate(R.layout.cablev2_ble_enable, container, false);
            if (BuildInfo.isAtLeastS()) {
                // Don't claim that Bluetooth will be switched off afterwards on Android 12 and
                // later because disabling Bluetooth requires an extra permission prompt.
                ((TextView) v.findViewById(R.id.cablev2_ble_enable_body)).setText("");
            }
            mSpinnerView = createSpinnerScreen(inflater, container);
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
                    v = createSpinnerScreen(inflater, container);
                    break;

                case QR:
                    // TODO: should check FEATURE_BLUETOOTH with
                    // https://developer.android.com/reference/android/content/pm/PackageManager.html#hasSystemFeature(java.lang.String)
                    // TODO: strings should be translated but this will be replaced during
                    // the UI process.

                    v = inflater.inflate(R.layout.cablev2_qr_scan, container, false);
                    mQRButton = v.findViewById(R.id.qr_scan);
                    mQRButton.setOnClickListener(this);

                    mHeader = v.findViewById(R.id.qr_image);
                    setHeader(R.style.idle);

                    mUnlinkButton = v.findViewById(R.id.unlink);
                    mUnlinkButton.setOnClickListener(this);
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
            mState = State.ENABLE_BLUETOOTH_REQUESTED;
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
                if (mAuthenticator != null) {
                    startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
                            ENABLE_BLUETOOTH_REQUEST_CODE);
                }
            }, BLE_SCREEN_DELAY_SECS * 1000);
        } else if (mState == State.BLUETOOTH_PERMISSION) {
            onBluetoothEnabled(/*needToDisableBluetooth=*/false);
        }
    }

    // Called when the Bluetooth adapter has been enabled, or was already enabled.
    private void onBluetoothEnabled(boolean needToDisableBluetooth) {
        // In Android 12 and above there is a new BLUETOOTH_ADVERTISE runtime permission.
        if (BuildInfo.isAtLeastS()) {
            maybeGetBluetoothPermission(needToDisableBluetooth);
            return;
        }

        onHaveBluetoothPermission(needToDisableBluetooth);
    }

    // Called on Android 12 or later after the Bluetooth adaptor is enabled.
    @TargetApi(31)
    private void maybeGetBluetoothPermission(boolean needToDisableBluetooth) {
        mState = State.BLUETOOTH_PERMISSION;
        final String advertise = permission.BLUETOOTH_ADVERTISE;

        if (getContext().checkSelfPermission(advertise) == PackageManager.PERMISSION_GRANTED) {
            onHaveBluetoothPermission(needToDisableBluetooth);
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
        mNeedToDisableBluetooth = needToDisableBluetooth;
        requestPermissions(new String[] {advertise}, 1);
    }

    // Called once the BLUETOOTH_ADVERTISE permission has been granted, or if
    // its not needed on this version of Android.
    private void onHaveBluetoothPermission(boolean needToDisableBluetooth) {
        mState = State.RUNNING;

        if (mMode == Mode.QR) {
            final String qrCode = mPendingQRCode;
            mPendingQRCode = null;
            mAuthenticator.onQRCode(qrCode, mPendingShouldLink);
        } else {
            mAuthenticator.onBluetoothReady(needToDisableBluetooth);
        }
    }

    /**
     * Updates the header image to be shown in a new "style". The Android
     * style system is used to make certain elements appear as a QR handshake
     * progresses.
     */
    private void setHeader(int style) {
        ContextThemeWrapper theme = new ContextThemeWrapper(getContext(), style);
        Drawable drawable =
                ResourcesCompat.getDrawable(getResources(), R.drawable.header, theme.getTheme());
        mHeader.setImageDrawable(drawable);
    }

    /**
     * Called when the button to scan a QR code is pressed.
     */
    @Override
    @SuppressLint("SetTextI18n")
    public void onClick(View v) {
        if (v == mUnlinkButton) {
            // TODO: localise strings.
            new AlertDialog.Builder(getContext())
                    .setTitle("Unlink all devices")
                    .setMessage("Do you want to unlink all previously connected devices?"
                            + " You will need to scan a QR code from a given device in"
                            + " order to use it again."
                            + " No credentials will be deleted.")
                    .setIcon(android.R.drawable.ic_dialog_alert)
                    .setPositiveButton(android.R.string.ok,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int whichButton) {
                                    mAuthenticator.unlinkAllDevices();
                                }
                            })
                    .setNegativeButton(android.R.string.cancel, null)
                    .show();
            return;
        } else if (v == mErrorCloseButton) {
            getActivity().finish();
            return;
        }

        assert (v == mQRButton);
        // If camera permission is already available, show the scanning
        // dialog.
        final Context context = getContext();
        if (mPermissionDelegate.hasPermission(permission.CAMERA)) {
            mState = State.QR_SCANNING;
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
            return;
        }

        // Otherwise prompt for permission first.
        if (mPermissionDelegate.canRequestPermission(permission.CAMERA)) {
            // The |Fragment| method |requestPermissions| is called rather than
            // the method on |mPermissionDelegate| because the latter routes the
            // |onRequestPermissionsResult| callback to the Activity, and not
            // this fragment.
            mState = State.CAMERA_PERMISSION;
            requestPermissions(new String[] {permission.CAMERA}, 1);
        } else {
            // TODO: permission cannot be requested on older versions of
            // Android. Does Chrome always get camera permission at install
            // time on those versions? If so, then this case should be
            // impossible.
        }
    }

    /**
     * Called when the camera has scanned a FIDO QR code.
     */
    @Override
    @SuppressLint("SetTextI18n")
    public void onQRCode(String value, boolean link) {
        assert mState == State.QR_SCANNING;
        setHeader(R.style.step1);

        mPendingQRCode = value;
        mPendingShouldLink = link;

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter.isEnabled()) {
            onBluetoothEnabled(/*needToDisableBluetooth=*/false);
        } else {
            mState = State.ENABLE_BLUETOOTH_REQUESTED;
            startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
                    ENABLE_BLUETOOTH_REQUEST_CODE);
        }
    }

    void onStatus(int code) {
        switch (mMode) {
            case QR:
                // These values must match up with the Status enum in v2_authenticator.h
                if (code == 1) {
                    setHeader(R.style.step2);
                } else if (code == 2) {
                    setHeader(R.style.step3);
                } else if (code == 3) {
                    setHeader(R.style.step4);
                }
                break;

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
            case CAMERA_PERMISSION:
                if (granted) {
                    assert permissions[0].equals(permission.CAMERA);
                    mState = State.QR_SCANNING;
                    (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
                } else {
                    mState = State.SETTINGS_UI;
                }
                break;

            case BLUETOOTH_PERMISSION_REQUESTED:
                final boolean needToDisable = mNeedToDisableBluetooth;
                mNeedToDisableBluetooth = false;

                if (granted) {
                    assert permissions[0].equals(permission.BLUETOOTH_ADVERTISE);
                    onHaveBluetoothPermission(needToDisable);
                } else {
                    mState = State.ERROR;

                    // If would be nice to disable Bluetooth here if we switched
                    // it on previously, but Android 12 requires more
                    // permissions to do that.

                    // TODO(agl): do a better UI once we're out of strings freeze.
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
                onBluetoothEnabled(/*needToDisableBluetooth=*/true);
                break;

            case FCM:
                ViewGroup top = (ViewGroup) getView();
                top.removeAllViews();
                top.addView(mSpinnerView);

                onBluetoothEnabled(/*needToDisableBluetooth=*/true);
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

        String desc;
        if (errorCode == ERROR_UNEXPECTED_EOF) {
            desc = getResources().getString(R.string.cablev2_error_timeout);
        } else {
            TextView errorCodeTextView = (TextView) mErrorView.findViewById(R.id.error_code);
            errorCodeTextView.setText(
                    getResources().getString(R.string.cablev2_error_code, errorCode));

            desc = getResources().getString(R.string.cablev2_error_generic);
        }

        TextView descriptionTextView = (TextView) mErrorView.findViewById(R.id.error_description);
        descriptionTextView.setText(desc);
    }
}
