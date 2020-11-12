// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.app.Activity;
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

import androidx.appcompat.app.AlertDialog;
import androidx.core.content.res.ResourcesCompat;
import androidx.fragment.app.Fragment;

import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * A fragment that provides a UI for scanning caBLE v2 QR codes.
 */
public class CableAuthenticatorUI
        extends Fragment implements OnClickListener, QRScanDialog.Callback {
    private enum Mode {
        QR, // Triggered from Settings; can scan QR code to start handshake.
        FCM, // Triggered by user selecting notification; handshake already running.
        USB, // Triggered by connecting via USB.
    }
    private Mode mMode;
    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;
    private LinearLayout mQRButton;
    private LinearLayout mUnlinkButton;
    private ImageView mHeader;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Context context = getContext();

        Bundle arguments = getArguments();
        final UsbAccessory accessory =
                (UsbAccessory) arguments.getParcelable(UsbManager.EXTRA_ACCESSORY);
        if (accessory != null) {
            mMode = Mode.USB;
        } else if (arguments.getBoolean("org.chromium.chrome.modules.cablev2_authenticator.FCM")) {
            mMode = Mode.FCM;
        } else {
            mMode = Mode.QR;
        }

        final long networkContext = arguments.getLong(
                "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext");
        final long registration =
                arguments.getLong("org.chromium.chrome.modules.cablev2_authenticator.Registration");
        final String activityClassName = arguments.getString(
                "org.chromium.chrome.modules.cablev2_authenticator.ActivityClassName");

        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
        mAuthenticator = new CableAuthenticator(getContext(), this, networkContext, registration,
                activityClassName, mMode == Mode.FCM, accessory);
    }

    @Override
    @SuppressLint("SetTextI18n")
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        getActivity().setTitle("Security Key");

        switch (mMode) {
            case USB:
                return inflater.inflate(R.layout.cablev2_usb_attached, container, false);

            case FCM:
                return inflater.inflate(R.layout.cablev2_fcm, container, false);

            case QR:
                // TODO: should check FEATURE_BLUETOOTH with
                // https://developer.android.com/reference/android/content/pm/PackageManager.html#hasSystemFeature(java.lang.String)
                // TODO: strings should be translated but this will be replaced during
                // the UI process.

                View v = inflater.inflate(R.layout.cablev2_qr_scan, container, false);
                mQRButton = v.findViewById(R.id.qr_scan);
                mQRButton.setOnClickListener(this);

                mHeader = v.findViewById(R.id.qr_image);
                setHeader(R.style.idle);

                mUnlinkButton = v.findViewById(R.id.unlink);
                mUnlinkButton.setOnClickListener(this);

                return v;
        }

        assert false;
        return null;
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
    public void onQRCode(String value) {
        setHeader(R.style.step1);
        mAuthenticator.onQRCode(value);
    }

    void onStatus(int code) {
        if (mMode != Mode.QR) {
            // In FCM mode, the handshake is done before the UI appears. For
            // USB everything should happen immediately.
            return;
        }

        // These values must match up with the Status enum in v2_authenticator.h
        if (code == 1) {
            setHeader(R.style.step2);
        } else if (code == 2) {
            setHeader(R.style.step3);
        } else if (code == 3) {
            setHeader(R.style.step4);
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
        mAuthenticator.onActivityResult(requestCode, resultCode, data);
    }

    @SuppressLint("SetTextI18n")
    void onAuthenticatorConnected() {}

    void onAuthenticatorResult(CableAuthenticator.Result result) {
        getActivity().runOnUiThread(() -> {
            // TODO: Temporary UI, needs i18n.
            String toast = "An error occured. Please try again.";
            switch (result) {
                case REGISTER_OK:
                    toast = "Registration succeeded";
                    break;
                case REGISTER_ERROR:
                    toast = "Registration failed";
                    break;
                case SIGN_OK:
                    toast = "Sign-in succeeded";
                    break;
                case SIGN_ERROR:
                    toast = "Sign-in failed";
                    break;
                case OTHER:
                    break;
            }
            Toast.makeText(getActivity(), toast, Toast.LENGTH_SHORT).show();
        });
    }

    void onComplete() {
        getActivity().runOnUiThread(() -> { getActivity().finish(); });
    }

    /**
     * onCloudMessage is called by {@link CableAuthenticatorModuleProvider} when a GCM message is
     * received.
     */
    public static void onCloudMessage(
            long event, long systemNetworkContext, long registration, String activityClassName) {
        CableAuthenticator.onCloudMessage(
                event, systemNetworkContext, registration, activityClassName);
    }
}
