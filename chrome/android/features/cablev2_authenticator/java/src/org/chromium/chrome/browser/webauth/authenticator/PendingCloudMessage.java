// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * PendingCloudMessage contains the information extracted from an FCM message that is pending
 * Bluetooth being enabled. It receives Bluetooth status updates and forwards the FCM message once
 * Bluetooth is on.
 */
class PendingCloudMessage extends BroadcastReceiver {
    private static final String TAG = "PendingCloudMessage";

    private final Context mContext;
    // The following four members store a pending cloud message while Bluetooth
    // is enabled.
    private final long mEvent;
    private final long mNetworkContext;
    private final long mRegistration;
    private final String mActivityClassName;
    private final byte[] mSecret;

    PendingCloudMessage(BluetoothAdapter adapter, long event, long systemNetworkContext,
            long registration, String activityClassName, byte[] secret) {
        super();

        mContext = ContextUtils.getApplicationContext();
        mEvent = event;
        mNetworkContext = systemNetworkContext;
        mRegistration = registration;
        mActivityClassName = activityClassName;
        mSecret = secret;

        mContext.registerReceiver(this, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));

        // Consent to enable Bluetooth in order to respond to requests was
        // obtained when scanning the QR code.
        if (adapter.enable()) {
            Log.i(TAG, "Cloud message is pending Bluetooth enabling");
            return;
        }

        // Bluetooth might have been enabled by another party between checking
        // and now.
        if (adapter.isEnabled()) {
            CableAuthenticator.onCloudMessage(mEvent, mNetworkContext, mRegistration,
                    mActivityClassName, mSecret, /*needToDisableBluetooth=*/false);
            return;
        }

        Log.i(TAG, "Bluetooth failed to enable. Dropping cloud message.");
        mContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Bundle extras = intent.getExtras();
        if (extras == null) {
            return;
        }
        int state = extras.getInt(BluetoothAdapter.EXTRA_STATE, -1);
        if (state == -1) {
            return;
        }

        switch (state) {
            case BluetoothAdapter.STATE_OFF:
                Log.i(TAG, "Bluetooth failed to enable. Dropping cloud message.");
                break;

            case BluetoothAdapter.STATE_ON:
                Log.i(TAG, "Bluetooth enabled. Forwarding cloud message.");
                CableAuthenticator.onCloudMessage(mEvent, mNetworkContext, mRegistration,
                        mActivityClassName, mSecret,
                        /*needToDisableBluetooth=*/true);
                break;

            default:
                // An intermediate state. Wait for the next message.
                return;
        }

        mContext.unregisterReceiver(this);
    }
}
