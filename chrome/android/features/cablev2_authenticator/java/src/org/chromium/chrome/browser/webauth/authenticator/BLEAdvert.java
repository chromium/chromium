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
        assert payload.length == 20;

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
                        .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                        .build();
        ParcelUuid fidoUuid = new ParcelUuid(UUID.fromString(CABLE_UUID));

        // The first 16 bytes of the payload are encoded into a 16-byte UUID.
        ByteBuffer bb = ByteBuffer.wrap(payload);
        long high = bb.getLong();
        long low = bb.getLong();
        final UUID uuid16 = new UUID(high, low);

        // The final four bytes of the payload are turned into a 4-byte UUID.
        // Depending on the value of those four bytes, this might happen to be a
        // 2-byte UUID, but the desktop handles that.
        high = (long) bb.getInt();
        high <<= 32;
        // This is the fixed suffix for short UUIDs in Bluetooth.
        high |= 0x1000;
        low = 0x800000805f9b34fbL;
        final UUID uuid4 = new UUID(high, low);

        AdvertiseData data = (new AdvertiseData.Builder())
                                     .addServiceUuid(fidoUuid)
                                     .addServiceUuid(new ParcelUuid(uuid16))
                                     .addServiceUuid(new ParcelUuid(uuid4))
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
