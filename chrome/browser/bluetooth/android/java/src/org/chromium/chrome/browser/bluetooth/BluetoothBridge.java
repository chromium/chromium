// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bluetooth;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Java access point for BluetoothBridge, allowing for querying Bluetooth state. */
public class BluetoothBridge {
    public static boolean isWebContentsConnectedToBluetoothDevice(WebContents webContents) {
        if (webContents == null) return false;
        return BluetoothBridgeJni.get().isWebContentsConnectedToBluetoothDevice(webContents);
    }

    public static boolean isWebContentsScanningForBluetoothDevices(WebContents webContents) {
        if (webContents == null) return false;
        return BluetoothBridgeJni.get().isWebContentsScanningForBluetoothDevices(webContents);
    }

    @NativeMethods
    interface Natives {
        boolean isWebContentsConnectedToBluetoothDevice(WebContents webContents);

        boolean isWebContentsScanningForBluetoothDevices(WebContents webContents);
    }
}
