// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;

import java.util.List;

/** A helper class to open Google Wallet. */
@NullMarked
public class GoogleWalletLauncher {

    // Google Wallet App package name.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";

    // Google Wallet main activity name.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_ACTIVITY_NAME =
            "com.google.commerce.tapandpay.android.wallet.WalletActivity";

    // Google Wallet URL for managing the user's passes.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_PASSES_URL = "https://wallet.google.com/wallet/passes";

    private GoogleWalletLauncher() {}

    /**
     * Opens Google Wallet. If Google Wallet App is installed on device, it opens the app and shows
     * the home page. Otherwise it opens the Google Wallet page in a Chrome Custom Tab.
     */
    public static void openGoogleWallet(Context context, PackageManager packageManager) {
        if (context == null) {
            return;
        }

        Intent walletIntent =
                new Intent().setClassName(GOOGLE_WALLET_PACKAGE_NAME, GOOGLE_WALLET_ACTIVITY_NAME);
        walletIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(walletIntent, 0);

        if (resolveInfos.isEmpty()) {
            CustomTabActivity.showInfoPage(context, GOOGLE_WALLET_PASSES_URL);
        } else {
            context.startActivity(walletIntent);
        }
    }
}
