// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.util.UUID;

class BLEAdvert implements Closeable {
    private static final String TAG = "CableBLEAdvert";
    // This UUID is allocated to Google.
    private static final String CABLE_UUID = "0000fde2-0000-1000-8000-00805f9b34fb";

    private AdvertiseCallback mCallback;

    BLEAdvert(byte[] payload) {
        BluetoothLeAdvertiser advertiser =
                BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        mCallback = new AdvertiseCallback() {
            @Override
            public void onStartFailure(int errorCode) {
                Log.i(TAG, "advertising failure " + errorCode);
            }

            @Override
            public void onStartSuccess(AdvertiseSettings settingsInEffect) {
                Log.i(TAG, "advertising success");
            }
        };

        AdvertiseSettings settings =
                (new AdvertiseSettings.Builder())
                        .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                        .setConnectable(false)
                        .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
                        .build();
        ParcelUuid fidoUuid = new ParcelUuid(UUID.fromString(CABLE_UUID));

        ByteBuffer bb = ByteBuffer.wrap(payload);
        long high = bb.getLong();
        long low = bb.getLong();
        UUID dataUuid = new UUID(high, low);

        AdvertiseData data = (new AdvertiseData.Builder())
                                     .addServiceUuid(fidoUuid)
                                     .addServiceUuid(new ParcelUuid(dataUuid))
                                     .setIncludeDeviceName(false)
                                     .setIncludeTxPowerLevel(false)
                                     .build();

        advertiser.startAdvertising(settings, data, mCallback);
    }

    @Override
    @CalledByNative
    public void close() {
        if (mCallback == null) {
            return;
        }

        BluetoothLeAdvertiser advertiser =
                BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        Log.i(TAG, "stopping advertising");
        advertiser.stopAdvertising(mCallback);
        mCallback = null;
    }
}
