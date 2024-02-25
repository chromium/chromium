// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
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
    public void launchActivityIfAllowed(
            Context context,
            Profile profile,
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode,
            @AccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        if (signinManager.isSigninAllowed()) {
            Intent intent =
                    SigninAndHistoryOptInActivity.createIntent(
                            context, noAccountSigninMode, historyOptInMode, accessPoint);
            context.startActivity(intent);
        }
        // TODO(https://crbug.com/1520783): Update the UI related to sign-in errors, and handle the
        // non-managed case.
        if (signinManager.isSigninDisabledByPolicy()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Signin.SigninDisabledNotificationShown", accessPoint, SigninAccessPoint.MAX);
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
    }
}
