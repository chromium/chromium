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
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * A fragment that provides a UI for scanning caBLE v2 QR codes.
 */
public class CableAuthenticatorUI extends Fragment
        implements OnClickListener, QRScanDialog.Callback, CableAuthenticator.Callback {
    /** True if this UI was created because the user connected a desktop via USB. */
    private boolean mCreatedByUsbIntent;

    private AndroidPermissionDelegate mPermissionDelegate;
    private CableAuthenticator mAuthenticator;

    private ButtonCompat mQRButton;
    private ProgressBar mSpinner;
    private TextView mStatus;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Context context = getContext();

        Bundle arguments = getArguments();
        final UsbAccessory accessory =
                (UsbAccessory) arguments.getParcelable(UsbManager.EXTRA_ACCESSORY);
        mCreatedByUsbIntent = (accessory != null);
        final long networkContext = arguments.getLong(
                "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext");
        final long instanceIdDriver = arguments.getLong(
                "org.chromium.chrome.modules.cablev2_authenticator.InstanceIDDriver");
        final String settingsActivityClassName = arguments.getString(
                "org.chromium.chrome.modules.cablev2_authenticator.SettingsActivityClassName");
        final String wrapperClassName = arguments.getString(
                "org.chromium.chrome.modules.cablev2_authenticator.WrapperClassName");
        final boolean isFcmNotification =
                arguments.getBoolean("org.chromium.chrome.modules.cablev2_authenticator.FCM");

        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) context));
        mAuthenticator =
                new CableAuthenticator(getContext(), this, networkContext, instanceIdDriver,
                        settingsActivityClassName, wrapperClassName, isFcmNotification, accessory);
    }

    @Override
    @SuppressLint("SetTextI18n")
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Shows a placeholder UI that provides a very basic animation and status text informing of
        // progress, as well as a button to scan QR codes.

        // TODO: should check FEATURE_BLUETOOTH with
        // https://developer.android.com/reference/android/content/pm/PackageManager.html#hasSystemFeature(java.lang.String)
        // TODO: strings should be translated but this will be replaced during
        // the UI process.
        getActivity().setTitle("Security Key");

        final Context context = getContext();

        ProgressBar mSpinner = new ProgressBar(context);
        mSpinner.setIndeterminate(true);
        mSpinner.setPadding(0, 60, 0, 60);

        mStatus = new TextView(context);
        mStatus.setPadding(0, 60, 0, 60);

        if (mCreatedByUsbIntent) {
            mStatus.setText("Connected via USB. Awaiting command.");
        } else {
            mStatus.setText("Looking for known devices nearby");
        }

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.addView(mSpinner);
        layout.addView(mStatus,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        if (!mCreatedByUsbIntent) {
            mQRButton = new ButtonCompat(context, R.style.TextButtonThemeOverlay);
            mQRButton.setText("Connect a new device");
            mQRButton.setOnClickListener(this);

            layout.addView(mQRButton,
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT));
        }

        return layout;
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
        mStatus.setText("Looking for your new device nearby");
        mAuthenticator.onQRCode(value);
    }

    /**
     * Called when camera permission has been requested and the user has resolved the permission
     * request.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        mPermissionDelegate = null;

        if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            (new QRScanDialog(this)).show(getFragmentManager(), "dialog");
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAuthenticator.close();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mAuthenticator.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    @SuppressLint("SetTextI18n")
    public void onAuthenticatorConnected() {
        getActivity().runOnUiThread(() -> {
            mStatus.setText("Connected. Verifying it's you.");
            mQRButton.setEnabled(false);
        });
    }

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
}
