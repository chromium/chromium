// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * SigninAndHistorySyncActivityLauncherImpl creates the proper intent and then launches the {@link
 * SigninAndHistorySyncActivity} in different scenarios.
 */
public final class SigninAndHistorySyncActivityLauncherImpl
        implements SigninAndHistorySyncActivityLauncher {
    private static SigninAndHistorySyncActivityLauncher sLauncher;

    /** Singleton instance getter */
    @MainThread
    public static SigninAndHistorySyncActivityLauncher get() {
        ThreadUtils.assertOnUiThread();
        if (sLauncher == null) {
            sLauncher = new SigninAndHistorySyncActivityLauncherImpl();
        }
        return sLauncher;
    }

    public static void setLauncherForTest(@Nullable SigninAndHistorySyncActivityLauncher launcher) {
        var oldValue = sLauncher;
        sLauncher = launcher;
        ResettersForTesting.register(() -> sLauncher = oldValue);
    }

    private SigninAndHistorySyncActivityLauncherImpl() {}

    @Override
    public boolean launchActivityIfAllowed(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @SigninAndHistorySyncCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistorySyncCoordinator.WithAccountSigninMode int withAccountSigninMode,
            @SigninAndHistorySyncCoordinator.HistoryOptInMode int historyOptInMode,
            @AccessPoint int accessPoint) {
        Intent intent =
                SigninAndHistorySyncActivity.createIntent(
                        context,
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        historyOptInMode,
                        accessPoint);
        return launchActivityOrShowError(context, profile, intent, historyOptInMode, accessPoint);
    }

    @Override
    public void launchActivityForHistorySyncDedicatedFlow(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @SigninAndHistorySyncCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistorySyncCoordinator.WithAccountSigninMode int withAccountSigninMode,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent =
                SigninAndHistorySyncActivity.createIntentForDedicatedFlow(
                        context,
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        signinAccessPoint);
        launchActivityOrShowError(
                context,
                profile,
                intent,
                SigninAndHistorySyncCoordinator.HistoryOptInMode.REQUIRED,
                signinAccessPoint);
    }

    private boolean launchActivityOrShowError(
            Context context,
            Profile profile,
            Intent intent,
            @SigninAndHistorySyncCoordinator.HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int accessPoint) {
        if (SigninAndHistorySyncCoordinator.willShowSigninUI(profile)
                || SigninAndHistorySyncCoordinator.willShowHistorySyncUI(
                        profile, historyOptInMode)) {
            context.startActivity(intent);
            return true;
        }
        // TODO(crbug.com/41493758): Update the UI related to sign-in errors, and handle the
        // non-managed case.
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        if (signinManager.isSigninDisabledByPolicy()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Signin.SigninDisabledNotificationShown", accessPoint, SigninAccessPoint.MAX);
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
        return false;
    }

    @Override
    public void launchUpgradePromoActivityIfAllowed(Context context, Profile profile) {
        if (SigninAndHistorySyncCoordinator.willShowSigninUI(profile)
                || SigninAndHistorySyncCoordinator.willShowHistorySyncUI(
                        profile, SigninAndHistorySyncCoordinator.HistoryOptInMode.OPTIONAL)) {
            Intent intent = SigninAndHistorySyncActivity.createIntentForUpgradePromo(context);
            context.startActivity(intent);
        }
    }
}
