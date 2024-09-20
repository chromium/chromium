// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;

/**
 * The controller used for user authentication (either with screen lock or biometrics) for devices
 * with Android V or higher.
 */
class MandatoryAuthenticatorControllerImpl extends DeviceAuthenticatorControllerImpl {

    public MandatoryAuthenticatorControllerImpl(Context context, Delegate delegate) {
        super(context, delegate);
    }

    @Override
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        // TODO(crbug.com/367683516): Implement.
        return BiometricsAvailability.OTHER_ERROR;
    }

    @Override
    public boolean canAuthenticateWithBiometricOrScreenLock() {
        // TODO(crbug.com/367683516): Implement.
        return false;
    }

    @RequiresApi(Build.VERSION_CODES.P)
    @Override
    public void authenticate() {
        // TODO(crbug.com/367683792): Implement.
    }
}
