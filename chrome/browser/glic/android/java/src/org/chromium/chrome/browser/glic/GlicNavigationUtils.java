// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Supplier;

/** Bridge between Java and native GLIC code to launch GLIC settings. */
@NullMarked
public class GlicNavigationUtils {
    private static @Nullable Supplier<SigninAndHistorySyncActivityLauncher> sLauncherSupplier;

    /**
     * Sets the launcher supplier used to trigger the bottom sheet sign-in and history sync flow.
     *
     * @param launcherSupplier The supplier for {@link SigninAndHistorySyncActivityLauncher}.
     */
    public static void setLauncher(
            Supplier<SigninAndHistorySyncActivityLauncher> launcherSupplier) {
        sLauncherSupplier = launcherSupplier;
    }

    private static @Nullable SigninAndHistorySyncActivityLauncher getLauncher() {
        return sLauncherSupplier != null ? sLauncherSupplier.get() : null;
    }

    /** Opens the GLIC settings page. */
    @CalledByNative
    private static void showGlicSettings() {
        Context context = ContextUtils.getApplicationContext();

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context, GlicSettings.class);
    }

    /**
     * Opens the GLIC sign in bottomsheet.
     *
     * @param profile The current user Profile.
     * @param webContents The WebContents of the page that triggered the sign-in.
     */
    @CalledByNative
    public static void showSignIn(Profile profile, WebContents webContents) {
        Activity activity = null;
        if (webContents.getTopLevelNativeWindow() != null) {
            activity = webContents.getTopLevelNativeWindow().getActivity().get();
        }
        if (activity == null) {
            activity = ApplicationStatus.getLastTrackedFocusedActivity();
        }
        if (activity == null) {
            return;
        }

        SigninAndHistorySyncActivityLauncher launcher = getLauncher();
        if (launcher == null) {
            return;
        }

        String title =
                activity.getResources()
                        .getString(R.string.signin_account_picker_bottom_sheet_title);
        String subtitle =
                activity.getResources()
                        .getString(R.string.signin_account_picker_bottom_sheet_benefits_subtitle);
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(title)
                        .setSubtitleString(subtitle)
                        .build();

        String syncTitle = activity.getResources().getString(R.string.history_sync_title);
        String syncSubtitle = activity.getResources().getString(R.string.history_sync_subtitle);
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                syncTitle,
                                syncSubtitle)
                        .build();

        Intent intent =
                launcher.createBottomSheetSigninIntentOrShowError(
                        activity, profile, config, SigninAccessPoint.GLIC_LAUNCH_BUTTON);
        if (intent != null) {
            activity.startActivity(intent);
        }
    }
}
