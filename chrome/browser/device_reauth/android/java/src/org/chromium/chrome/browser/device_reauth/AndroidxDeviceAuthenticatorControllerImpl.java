// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

class AndroidxDeviceAuthenticatorControllerImpl implements DeviceAuthenticatorController {

    @Override
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        // TODO(crbug.com/340473460): Implement.
        return BiometricsAvailability.NOT_ENROLLED;
    }

    @Override
    public boolean canAuthenticateWithBiometricOrScreenLock() {
        // TODO(crbug.com/340473460): Implement.
        return false;
    }

    @Override
    public void authenticate() {
        // TODO(crbug.com/340473460): Implement.
    }

    @Override
    public void cancel() {
        // TODO(crbug.com/340473460): Implement.
    }
}
