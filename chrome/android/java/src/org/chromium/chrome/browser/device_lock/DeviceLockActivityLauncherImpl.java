// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_lock;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.ui.base.WindowAndroid;

/**
 * DeviceLockActivityLauncher creates the proper intent and then launches the
 * {@link DeviceLockActivity} in different scenarios.
 */
public class DeviceLockActivityLauncherImpl implements DeviceLockActivityLauncher {
    private static DeviceLockActivityLauncherImpl sLauncher;

    /**
     * Singleton instance getter
     */
    public static DeviceLockActivityLauncherImpl get() {
        if (sLauncher == null) {
            sLauncher = new DeviceLockActivityLauncherImpl();
        }
        return sLauncher;
    }

    private DeviceLockActivityLauncherImpl() {}

    @Override
    public void launchDeviceLockActivity(Context context, @Nullable String selectedAccount,
            WindowAndroid windowAndroid, WindowAndroid.IntentCallback callback) {
        Intent intent = DeviceLockActivity.createIntent(context, selectedAccount);
        windowAndroid.showIntent(intent, callback, null);
    }
}
