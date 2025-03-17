// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.components.payments.AndroidIntentLauncher;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * The implementation of Android intent launcher that uses WindowAndroid for sending the "PAY"
 * intent to invoke Android payment apps.
 */
/* package */ class WindowAndroidIntentLauncher implements AndroidIntentLauncher {
    private final WebContents mWebContents;

    /**
     * @param webContents The web contents whose WindowAndroid should be used for invoking Android
     *     payment apps and receiving the result.
     */
    /* package */ WindowAndroidIntentLauncher(WebContents webContents) {
        mWebContents = webContents;
    }

    // Launcher implementation.
    @Override
    public void launchPaymentApp(
            Intent intent,
            Callback<String> errorCallback,
            WindowAndroid.IntentCallback intentCallback) {
        if (mWebContents.isDestroyed()) {
            errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window == null) {
            errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        try {
            if (!window.showIntent(intent, intentCallback, R.string.payments_android_app_error)) {
                errorCallback.onResult(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            }
        } catch (SecurityException e) {
            // Payment app does not have android:exported="true" on the PAY activity.
            errorCallback.onResult(ErrorStrings.PAYMENT_APP_PRIVATE_ACTIVITY);
        }
    }
}
