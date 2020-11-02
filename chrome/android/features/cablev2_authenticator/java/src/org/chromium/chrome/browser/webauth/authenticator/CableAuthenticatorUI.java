// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.fragment.app.Fragment;

import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * A fragment that provides a UI for scanning caBLE v2 QR codes.
 */
public class CableAuthenticatorUI extends Fragment
        implements OnClickListener, QRScanDialog.Callback, CableAuthenticator.Callback {
    private enum Mode {
        QR, // Triggered from Settings; can scan QR code to start handshake.
        FCM, // Triggered by user selecting notification; handshake already running.
        USB, // Triggered by connecting via USB.
    }
    private Mode mMode;
    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;
    private LinearLayout mQRButton;

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

                return v;
        }

        assert false;
        return null;
    }

    /**
     * Called when the button to scan a QR code is pressed.
     */
    @Override
    public void onClick(View v) {
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
        mAuthenticator.onQRCode(value);
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

    @Override
    @SuppressLint("SetTextI18n")
    public void onAuthenticatorConnected() {}

    @Override
    public void onAuthenticatorResult(CableAuthenticator.Result result) {
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

    @Override
    public void onComplete() {
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
