// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.permissions.BluetoothScanningPromptAndroidDelegate;

/**
 *  The implementation of {@link BluetoothScanningPromptAndroidDelegate} for Chrome.
 */
public class ChromeBluetoothScanningPromptAndroidDelegate
        implements BluetoothScanningPromptAndroidDelegate {

    @CalledByNative
    private static ChromeBluetoothScanningPromptAndroidDelegate create() {
        return new ChromeBluetoothScanningPromptAndroidDelegate();
    }
}
