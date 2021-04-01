// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
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

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
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

    // USB_PROMPT_TIMEOUT_SECS is the number of seconds the spinner will show
    // for before being replaced with a prompt to connect via USB cable.
    private static final int USB_PROMPT_TIMEOUT_SECS = 20;

    private static final String ACTIVITY_CLASS_NAME_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.ActivityClassName";
    private static final String FCM_EXTRA = "org.chromium.chrome.modules.cablev2_authenticator.FCM";
    private static final String NETWORK_CONTEXT_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String SECRET_EXTRA =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";
    private static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";

    private enum Mode {
        QR, // Triggered from Settings; can scan QR code to start handshake.
        FCM, // Triggered by user selecting notification; handshake already running.
        USB, // Triggered by connecting via USB.
        SERVER_LINK, // Triggered by GMSCore forwarding from GAIA.
    }
    private Mode mMode;
    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;
    private LinearLayout mQRButton;
    private LinearLayout mUnlinkButton;
    private ImageView mHeader;
    private TextView mStatusText;
    private View mErrorView;
    private View mErrorCloseButton;

    // The following two members store a pending QR-scan result while Bluetooth
    // is enabled.
    private String mPendingQRCode;
    private boolean mPendingShouldLink;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Context context = getContext();

        Bundle arguments = getArguments();
        final UsbAccessory accessory =
                (UsbAccessory) arguments.getParcelable(UsbManager.EXTRA_ACCESSORY);
        final byte[] serverLink = arguments.getByteArray(SERVER_LINK_EXTRA);
        if (accessory != null) {
            mMode = Mode.USB;
        } else if (arguments.getBoolean(FCM_EXTRA)) {
            mMode = Mode.FCM;
        } else if (serverLink != null) {
            mMode = Mode.SERVER_LINK;
        } else {
            mMode = Mode.QR;
        }

        Log.i(TAG, "Starting in mode " + mMode.toString());

        final long networkContext = arguments.getLong(NETWORK_CONTEXT_EXTRA);
        final long registration = arguments.getLong(REGISTRATION_EXTRA);
        final String activityClassName = arguments.getString(ACTIVITY_CLASS_NAME_EXTRA);
        final byte[] secret = arguments.getByteArray(SECRET_EXTRA);

        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
        mAuthenticator = new CableAuthenticator(getContext(), this, networkContext, registration,
                activityClassName, secret, mMode == Mode.FCM, accessory, serverLink);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        getActivity().setTitle(R.string.cablev2_activity_title);
        ViewGroup top = new LinearLayout(getContext());

        // Inflate the error view in case it's needed later.
        mErrorView = inflater.inflate(R.layout.cablev2_error, container, false);

        View v = null;
        switch (mMode) {
            case USB:
                v = inflater.inflate(R.layout.cablev2_usb_attached, container, false);
                break;

            case FCM:
                v = inflater.inflate(R.layout.cablev2_fcm, container, false);
                break;

            case SERVER_LINK:
                v = inflater.inflate(R.layout.cablev2_serverlink, container, false);
                mStatusText = v.findViewById(R.id.status_text);

                ImageView spinner = (ImageView) v.findViewById(R.id.spinner);
                final AnimatedVectorDrawableCompat anim = AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.circle_loader_animation);
                // There is no way to make an animation loop. Instead it must be
                // manually started each time it completes.
                anim.registerAnimationCallback(new Animatable2Compat.AnimationCallback() {
                    @Override
                    public void onAnimationEnd(Drawable drawable) {
                        if (drawable != null && drawable.isVisible()) {
                            anim.start();
                        }
                    }
                });
                spinner.setImageDrawable(anim);
                anim.start();
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

            default:
                assert false;
        }

        top.addView(v);
        return top;
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
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
            return;
        }

        // Otherwise prompt for permission first.
        if (mPermissionDelegate.canRequestPermission(permission.CAMERA)) {
            // The |Fragment| method |requestPermissions| is called rather than
            // the method on |mPermissionDelegate| because the latter routes the
            // |onRequestPermissionsResult| callback to the Activity, and not
            // this fragment.
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
        setHeader(R.style.step1);

        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter.isEnabled()) {
            mAuthenticator.onQRCode(value, link);
        } else {
            mPendingQRCode = value;
            mPendingShouldLink = link;
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
                // These values must match up with the Status enum in v2_authenticator.h
                int id = -1;
                if (code == 1) {
                    id = R.string.cablev2_serverlink_status_connecting;
                } else if (code == 2) {
                    id = R.string.cablev2_serverlink_status_connected;
                } else if (code == 3) {
                    id = R.string.cablev2_serverlink_status_processing;
                } else {
                    break;
                }

                mStatusText.setText(getResources().getString(id));
                break;

            case FCM:
            case USB:
                // In FCM mode, the handshake is done before the UI appears. For
                // USB everything should happen immediately.
        }
    }

    /**
     * Called when camera permission has been requested and the user has resolved the permission
     * request.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        mAuthenticator.close();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == ENABLE_BLUETOOTH_REQUEST_CODE) {
            String qrCode = mPendingQRCode;
            mPendingQRCode = null;
            mAuthenticator.onQRCode(qrCode, mPendingShouldLink);
            return;
        }

        mAuthenticator.onActivityResult(requestCode, resultCode, data);
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

        mErrorCloseButton = mErrorView.findViewById(R.id.error_close);
        mErrorCloseButton.setOnClickListener(this);

        String desc;
        if (errorCode == 100 /* cablev2::authenticator::Platform::Error::UNEXPECTED_EOF */) {
            desc = getResources().getString(R.string.cablev2_error_timeout);
        } else {
            TextView errorCodeTextView = (TextView) mErrorView.findViewById(R.id.error_code);
            errorCodeTextView.setText(
                    getResources().getString(R.string.cablev2_error_code, errorCode));

            desc = getResources().getString(R.string.cablev2_error_generic);
        }

        TextView descriptionTextView = (TextView) mErrorView.findViewById(R.id.error_description);
        descriptionTextView.setText(desc);

        ViewGroup top = (ViewGroup) getView();
        top.removeAllViews();
        top.addView(mErrorView);
    }

    /**
     * onCloudMessage is called by {@link CableAuthenticatorModuleProvider} when a GCM message is
     * received.
     */
    public static void onCloudMessage(long event, long systemNetworkContext, long registration,
            String activityClassName, byte[] secret) {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter.isEnabled()) {
            CableAuthenticator.onCloudMessage(event, systemNetworkContext, registration,
                    activityClassName, secret, /*needToDisableBluetooth=*/false);
            return;
        }

        new PendingCloudMessage(
                adapter, event, systemNetworkContext, registration, activityClassName, secret);
    }
}
