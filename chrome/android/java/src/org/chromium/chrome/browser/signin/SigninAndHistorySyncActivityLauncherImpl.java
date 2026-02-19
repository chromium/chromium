// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.MainThread;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/**
 * SigninAndHistorySyncActivityLauncherImpl creates the proper intent and then launches the {@link
 * SigninAndHistorySyncActivity} in different scenarios.
 */
@NullMarked
public final class SigninAndHistorySyncActivityLauncherImpl
        implements SigninAndHistorySyncActivityLauncher {
    private static @Nullable SigninAndHistorySyncActivityLauncher sLauncher;

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
            Context context,
            Profile profile,
            BottomSheetSigninAndHistorySyncConfig config,
            @AccessPoint int accessPoint) {

        if (SigninAndHistorySyncCoordinator.canStartSigninAndHistorySyncOrShowError(
                context, profile, config.historyOptInMode, accessPoint)) {
            return SigninAndHistorySyncActivity.createIntent(context, config, accessPoint);
        }

        return null;
    }

    @MainThread
    @Override
    public BottomSheetSigninAndHistorySyncCoordinator
            createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                    WindowAndroid windowAndroid,
                    Activity activity,
                    ActivityResultTracker activityResultTracker,
                    BottomSheetSigninAndHistorySyncCoordinator.Delegate delegate,
                    DeviceLockActivityLauncher deviceLockActivityLauncher,
                    OneshotSupplier<Profile> profileSupplier,
                    Supplier<BottomSheetController> bottomSheetController,
                    Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
                    SnackbarManager snackbarManager,
                    @SigninAccessPoint int signinAccessPoint) {
        return BottomSheetSigninAndHistorySyncCoordinator.createAndObserveAddAccountResult(
                windowAndroid,
                activity,
                activityResultTracker,
                delegate,
                deviceLockActivityLauncher,
                profileSupplier,
                bottomSheetController,
                modalDialogManagerSupplier,
                snackbarManager,
                signinAccessPoint);
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
        if (SigninAndHistorySyncCoordinator.canStartSigninAndHistorySyncOrShowError(
                context, profile, config.historyOptInMode, signinAccessPoint)) {
            return SigninAndHistorySyncActivity.createIntentForFullscreenSignin(
                    context, config, signinAccessPoint);
        }
        return null;
    }
}
