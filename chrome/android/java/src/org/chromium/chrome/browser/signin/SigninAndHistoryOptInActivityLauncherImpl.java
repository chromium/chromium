// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * SigninAndHistoryOptInActivityLauncherImpl creates the proper intent and then launches the {@link
 * SigninAndHistoryOptInActivity} in different scenarios.
 */
public final class SigninAndHistoryOptInActivityLauncherImpl
        implements SigninAndHistoryOptInActivityLauncher {
    private static SigninAndHistoryOptInActivityLauncher sLauncher;

    /** Singleton instance getter */
    @MainThread
    public static SigninAndHistoryOptInActivityLauncher get() {
        ThreadUtils.assertOnUiThread();
        if (sLauncher == null) {
            sLauncher = new SigninAndHistoryOptInActivityLauncherImpl();
        }
        return sLauncher;
    }

    public static void setLauncherForTest(
            @Nullable SigninAndHistoryOptInActivityLauncher launcher) {
        var oldValue = sLauncher;
        sLauncher = launcher;
        ResettersForTesting.register(() -> sLauncher = oldValue);
    }

    private SigninAndHistoryOptInActivityLauncherImpl() {}

    @Override
    public boolean launchActivityIfAllowed(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.WithAccountSigninMode int withAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode,
            @AccessPoint int accessPoint) {
        Intent intent =
                SigninAndHistoryOptInActivity.createIntent(
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
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.WithAccountSigninMode int withAccountSigninMode,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent =
                SigninAndHistoryOptInActivity.createIntentForDedicatedFlow(
                        context,
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        signinAccessPoint);
        launchActivityOrShowError(
                context,
                profile,
                intent,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.REQUIRED,
                signinAccessPoint);
    }

    private boolean launchActivityOrShowError(
            Context context,
            Profile profile,
            Intent intent,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int accessPoint) {
        if (SigninAndHistoryOptInCoordinator.willShowSigninUI(profile)
                || SigninAndHistoryOptInCoordinator.willShowHistorySyncUI(
                        profile, historyOptInMode)) {
            // Set fade-in animation for the sign-in flow.
            Bundle startActivityOptions =
                    ActivityOptionsCompat.makeCustomAnimation(
                                    context, android.R.anim.fade_in, R.anim.no_anim)
                            .toBundle();
            context.startActivity(intent, startActivityOptions);
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
        if (SigninAndHistoryOptInCoordinator.willShowSigninUI(profile)
                || SigninAndHistoryOptInCoordinator.willShowHistorySyncUI(
                        profile, SigninAndHistoryOptInCoordinator.HistoryOptInMode.REQUIRED)) {
            Intent intent = SigninAndHistoryOptInActivity.createIntentForUpgradePromo(context);
            context.startActivity(intent);
        }
    }
}
