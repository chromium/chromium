// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.permissions.BluetoothChooserAndroidDelegate;

/**
 *  The implementation of {@link BluetoothChooserAndroidDelegate} for Chrome.
 */
public class ChromeBluetoothChooserAndroidDelegate implements BluetoothChooserAndroidDelegate {

    @CalledByNative
    private static ChromeBluetoothChooserAndroidDelegate create() {
        return new ChromeBluetoothChooserAndroidDelegate();
    }
}
