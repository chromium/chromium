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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.widget.Toast;

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
    public @Nullable Intent createBottomSheetSigninIntentOrShowError(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull BottomSheetSigninAndHistorySyncConfig config,
            @AccessPoint int accessPoint) {

        if (canStartSigninAndHistorySyncOrShowError(
                context, profile, config.historyOptInMode, accessPoint)) {
            return SigninAndHistorySyncActivity.createIntent(context, config, accessPoint);
        }

        return null;
    }

    private boolean canStartSigninAndHistorySyncOrShowError(
            Context context,
            Profile profile,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @SigninAccessPoint int accessPoint) {
        if (SigninAndHistorySyncCoordinator.willShowSigninUi(profile)
                || SigninAndHistorySyncCoordinator.willShowHistorySyncUi(
                        profile, historyOptInMode)) {
            return true;
        }
        // TODO(crbug.com/354912290): Update the UI related to sign-in errors.
        if (UserPrefs.get(profile).isManagedPreference(Pref.SIGNIN_ALLOWED)) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Signin.SigninDisabledNotificationShown",
                    accessPoint,
                    SigninAccessPoint.MAX_VALUE);
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        } else {
            Toast.makeText(
                            context,
                            context.getString(
                                    R.string.signin_account_picker_bottom_sheet_error_title),
                            Toast.LENGTH_LONG)
                    .show();
        }
        return false;
    }

    @Override
    public @Nullable Intent createFullscreenSigninIntent(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        if (SigninAndHistorySyncCoordinator.willShowSigninUi(profile)
                || SigninAndHistorySyncCoordinator.willShowHistorySyncUi(
                        profile, config.historyOptInMode)) {
            return SigninAndHistorySyncActivity.createIntentForFullscreenSignin(
                    context, config, signinAccessPoint);
        }
        return null;
    }

    @Override
    public @Nullable Intent createFullscreenSigninIntentOrShowError(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        if (canStartSigninAndHistorySyncOrShowError(
                context, profile, config.historyOptInMode, signinAccessPoint)) {
            return SigninAndHistorySyncActivity.createIntentForFullscreenSignin(
                    context, config, signinAccessPoint);
        }
        return null;
    }
}
