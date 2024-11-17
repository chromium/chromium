// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.plus_addresses.PlusAddressesMetricsRecorder;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.WindowAndroid;

/** A helper class to open plus address management UI. */
@JNINamespace("plus_addresses")
public class PlusAddressesHelper {
    // GMS Core accountsettings screen that comes from the resource_id.proto.
    private static final int PLUS_ADDRESS_EMAIL_SCREEN = 10764;

    @CalledByNative
    public static void openManagePlusAddresses(WindowAndroid window, Profile profile) {
        Activity activity = window.getActivity().get();

        if (activity == null) {
            return;
        }

        openManagePlusAddresses(activity, profile);
    }

    public static void openManagePlusAddresses(Activity activity, Profile profile) {
        @Nullable
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) {
            // There's no identity manager in the incognito mode. Make sure management UI is not
            // opened in the incognito mode.
            return;
        }

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE)) {
            openManagePlusAddressesInCct(activity);
            return;
        }

        @Nullable
        CoreAccountInfo accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo == null) {
            // Signed out users should not be able to open the plus address management UI. Early
            // return if account information is not available for whatever reason.
            return;
        }

        try {
            activity.startActivityForResult(createAccountSettingsIntent(accountInfo.getEmail()), 0);
            PlusAddressesMetricsRecorder.recordAccountSettingsLaunched(/* success= */ true);
        } catch (ActivityNotFoundException e) {
            // Default to opening the management page in the Chrome custom tab.
            openManagePlusAddressesInCct(activity);
            PlusAddressesMetricsRecorder.recordAccountSettingsLaunched(/* success= */ false);
        }
    }

    private static Intent createAccountSettingsIntent(String primaryEmail) {
        return new Intent("com.google.android.gms.accountsettings.action.VIEW_SETTINGS")
                .setPackage("com.google.android.gms")
                .putExtra("extra.screenId", PLUS_ADDRESS_EMAIL_SCREEN)
                .putExtra("extra.accountName", primaryEmail)
                .putExtra("extra.utmSource", "chrome")
                .putExtra("extra.campaign", "chrome");
    }

    private static void openManagePlusAddressesInCct(Context context) {
        CustomTabActivity.showInfoPage(context, getPlusAddressManagementUrl());
    }

    private static String getPlusAddressManagementUrl() {
        return PlusAddressesHelperJni.get().getPlusAddressManagementUrl();
    }

    private PlusAddressesHelper() {}

    @NativeMethods
    interface Natives {
        String getPlusAddressManagementUrl();
    }
}
