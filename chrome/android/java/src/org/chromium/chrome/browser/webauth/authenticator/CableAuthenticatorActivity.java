// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.app.ActivityManager;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.util.Base64;

import androidx.fragment.app.Fragment;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.webauthn.CableAuthenticatorModuleProvider;

/**
 * Phone as a Security Key activity.
 *
 * This activity lives in the main APK and is the target for:
 *   1. Notifications triggered by cloud messages telling us that an authentication
 *      is pending.
 *   2. A USB host telling the device that it wishes to speak CTAP2 over AOA.
 *      (See https://source.android.com/devices/accessories/aoa.)
 *
 * It hosts the {@link Fragment} that drives the security key process, which
 * pulls in the dynamic feature module containing the needed code.
 */
public class CableAuthenticatorActivity extends ChromeBaseAppCompatActivity {
    private static final String TAG = "CableAuthenticatorActivity";
    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    // See https://developer.android.com/guide/topics/connectivity/usb/accessory#java
    static final String USB_ACCESSORY_ATTACHED =
            "android.hardware.usb.action.USB_ACCESSORY_ATTACHED";
    static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTitle("Phone as a Security Key");

        // Ensure that the full browser is running since this activity may be
        // triggered by a USB message.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        super.onCreate(savedInstanceState);

        onNewIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        Bundle arguments;
        if (intent.getAction() != null && intent.getAction().equals(USB_ACCESSORY_ATTACHED)) {
            // This can be triggered by an implicit intent if a desktop
            // Chrome is connected via USB. This is used to expose the
            // phone's security key to the desktop.
            UsbAccessory accessory =
                    (UsbAccessory) intent.getParcelableExtra(UsbManager.EXTRA_ACCESSORY);

            // The specific extra of interest is filtered out rather than
            // passing intent's whole Bundle because the external Intent
            // is untrusted.
            arguments = new Bundle();
            arguments.putParcelable(UsbManager.EXTRA_ACCESSORY, accessory);
        } else if (intent.hasExtra(SERVER_LINK_EXTRA)) {
            // This Intent comes from GMSCore when it's triggering a server-linked connection.
            final String serverLinkBase64 = intent.getStringExtra(SERVER_LINK_EXTRA);
            arguments = new Bundle();
            try {
                final byte[] serverLink = Base64.decode(serverLinkBase64, Base64.DEFAULT);
                arguments.putByteArray(SERVER_LINK_EXTRA, serverLink);
            } catch (IllegalArgumentException e) {
                Log.i(TAG, "Invalid base64 in ServerLink argument");
            }
        } else {
            // Since this Activity is not otherwise exported, this only happens when a notification
            // is tapped and |EXTRA_SHOW_FRAGMENT_ARGUMENTS| thus comes from our own PendingIntent.
            arguments = intent.getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        }

        Fragment fragment = new CableAuthenticatorModuleProvider();
        fragment.setArguments(arguments);

        getSupportFragmentManager()
                .beginTransaction()
                .replace(android.R.id.content, fragment)
                .commit();

        Resources res = getResources();
        setTaskDescription(new ActivityManager.TaskDescription(res.getString(R.string.app_name),
                BitmapFactory.decodeResource(res, R.mipmap.app_icon),
                ApiCompatibilityUtils.getColor(res, R.color.default_primary_color)));
    }
}
