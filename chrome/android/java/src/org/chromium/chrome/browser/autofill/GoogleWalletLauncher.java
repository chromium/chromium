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

    // GMS Core package name to open Google Wallet passes settings.
    @VisibleForTesting public static final String GMS_CORE_PACKAGE_NAME = "com.google.android.gms";

    // Google Wallet main activity name.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_ACTIVITY_NAME =
            "com.google.commerce.tapandpay.android.wallet.WalletActivity";

    // Google Pay activity name to open Google Wallet passes settings.
    @VisibleForTesting
    public static final String GOOGLE_PAY_ACTIVITY_NAME =
            "com.google.android.gms.pay.main.PayActivity";

    // An action to view Google Wallet passes settings page.
    @VisibleForTesting
    public static final String VIEW_MANAGE_PASSES_ACTION =
            "com.google.android.gms.pay.settings.VIEW_MANAGE_PASSES";

    // Google Wallet URL for managing the user's passes.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_PASSES_URL = "https://wallet.google.com/wallet/passes";

    // Google Wallet URL for manage passes settings.
    @VisibleForTesting
    public static final String GOOGLE_WALLET_MANAGE_PASSES_DATA_URL =
            "https://wallet.google.com/wallet/settings/managepassesdata";

    private GoogleWalletLauncher() {}

    /**
     * Opens Google Wallet. If Google Wallet App is installed on device, it opens the app and shows
     * the home page. Otherwise it opens the Google Wallet page in a Chrome Custom Tab.
     *
     * @param context The current application context.
     * @param packageManager The current application package manager.
     */
    public static void openGoogleWallet(Context context, PackageManager packageManager) {
        Intent walletIntent =
                new Intent().setClassName(GOOGLE_WALLET_PACKAGE_NAME, GOOGLE_WALLET_ACTIVITY_NAME);
        walletIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        maybeLaunchIntent(context, packageManager, walletIntent, GOOGLE_WALLET_PASSES_URL);
    }

    /**
     * Opens passes settings in the Google Wallet. If Google Wallet App is installed on device, it
     * opens the app and shows the home page in the app. Otherwise it opens the Google Wallet passes
     * settings page in a Chrome Custom Tab.
     *
     * @param context The current application context.
     * @param packageManager The current application package manager.
     */
    public static void openGoogleWalletPassesSettings(
            Context context, PackageManager packageManager) {
        Intent walletIntent =
                new Intent(VIEW_MANAGE_PASSES_ACTION)
                        .setPackage(GMS_CORE_PACKAGE_NAME)
                        .setClassName(GMS_CORE_PACKAGE_NAME, GOOGLE_PAY_ACTIVITY_NAME);
        walletIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        maybeLaunchIntent(
                context, packageManager, walletIntent, GOOGLE_WALLET_MANAGE_PASSES_DATA_URL);
    }

    private static void maybeLaunchIntent(
            Context context, PackageManager packageManager, Intent intent, String fallbackUrl) {
        List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(intent, 0);

        if (resolveInfos.isEmpty()) {
            CustomTabActivity.showInfoPage(context, fallbackUrl);
        } else {
            context.startActivity(intent);
        }
    }
}
