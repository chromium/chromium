// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.ParcelUuid;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;

import java.io.Closeable;
import java.util.UUID;

class BLEAdvert implements Closeable {
    private static final String TAG = "CableBLEAdvert";
    // This UUID is allocated to Google.
    private static final String CABLE_UUID = "0000fff9-0000-1000-8000-00805f9b34fb";

    private AdvertiseCallback mCallback;

    BLEAdvert(byte[] payload) {
        assert payload.length == 20;

        BluetoothLeAdvertiser advertiser =
                BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        if (advertiser == null) {
            // It's possible for the Bluetooth adapter to have been disabled
            // between the previous check and now (crbug.com/123454675). Due to
            // the narrowness of this corner case we don't attempt to watch the
            // adapter in case it is reenabled. Instead the advert is lost and
            // the transaction won't work.
            return;
        }

        mCallback =
                new AdvertiseCallback() {
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
                new AdvertiseSettings.Builder()
                        .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                        .setConnectable(false)
                        .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                        .build();
        ParcelUuid fidoUuid = new ParcelUuid(UUID.fromString(CABLE_UUID));
        AdvertiseData data =
                new AdvertiseData.Builder()
                        .addServiceUuid(fidoUuid)
                        .addServiceData(fidoUuid, payload)
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
        // The Bluetooth adapter may have been disabled during the transaction.
        if (advertiser != null) {
            Log.i(TAG, "stopping advertising");
            advertiser.stopAdvertising(mCallback);
        }
        mCallback = null;
    }
}
