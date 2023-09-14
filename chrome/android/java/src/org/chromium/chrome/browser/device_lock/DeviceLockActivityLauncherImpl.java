// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_lock;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.device_reauth.DeviceAuthRequester;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
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

    /**
     * Set the shared instance for testing.
     *
     * @param deviceLockActivityLauncherImpl The {@link DeviceLockActivityLauncherImpl} instance to
     *        use for testing.
     */
    public static void setInstanceForTesting(
            DeviceLockActivityLauncherImpl deviceLockActivityLauncherImpl) {
        sLauncher = deviceLockActivityLauncherImpl;
    }

    /**
     * Resets the shared instance used for testing.
     */
    public static void resetInstanceForTesting() {
        sLauncher = null;
    }

    private DeviceLockActivityLauncherImpl() {}

    @Override
    public void launchDeviceLockActivity(Context context, @Nullable String selectedAccount,
            WindowAndroid windowAndroid, WindowAndroid.IntentCallback callback) {
        Intent intent = DeviceLockActivity.createIntent(context, selectedAccount);
        windowAndroid.showIntent(intent, callback, null);
    }

    // TODO(crbug.com/1482534)
    // Refactor to use DeviceLockDialogController rather than #launchDeviceLockActivity.
    @Override
    public void presentDeviceLockChallenge(
            Context context, WindowAndroid windowAndroid, Runnable callback) {
        if (shouldShowDeviceLockPage(context)) {
            launchDeviceLockActivity(
                    context, null, windowAndroid, (int resultCode, Intent data) -> {
                        if (resultCode == Activity.RESULT_OK) {
                            callback.run();
                        }
                    });
        } else {
            ReauthenticatorBridge.create(DeviceAuthRequester.DEVICE_LOCK_PAGE)
                    .reauthenticate((authSucceeded) -> {
                        if (authSucceeded) {
                            callback.run();
                        }
                    }, false);
        }
    }

    private static boolean shouldShowDeviceLockPage(Context context) {
        boolean deviceLockPageHasBeenPassed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false);
        boolean deviceLockIsPresent =
                ((KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE))
                        .isDeviceSecure();
        return !deviceLockPageHasBeenPassed || !deviceLockIsPresent;
    }
}
