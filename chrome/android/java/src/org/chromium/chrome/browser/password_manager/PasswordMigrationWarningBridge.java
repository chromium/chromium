// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.password_manager.settings.ExportFlow;
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.password_manager.settings.PasswordManagerHandlerProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningCoordinator;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningTriggers;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinator;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinatorFactory;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** The bridge that is used to show the password migration warning. */
class PasswordMigrationWarningBridge {
    @CalledByNative
    static void showWarning(
            WindowAndroid windowAndroid,
            Profile profile,
            @PasswordMigrationWarningTriggers int referrer) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return;
        Context context = windowAndroid.getContext().get();
        if (context == null) return;
        // The export flow won't work unless the sheet is started with an Activity as a Context.
        if (ContextUtils.activityFromContext(context) == null) return;
        showWarningInternal(context, bottomSheetController, profile, referrer);
    }

    @CalledByNative
    static void showWarningWithActivity(
            Activity activity,
            BottomSheetController bottomSheetController,
            Profile profile,
            @PasswordMigrationWarningTriggers int referrer) {
        showWarningInternal(activity, bottomSheetController, profile, referrer);
    }

    private static void showWarningInternal(
            Context context,
            BottomSheetController bottomSheetController,
            Profile profile,
            @PasswordMigrationWarningTriggers int referrer) {
        PasswordMigrationWarningCoordinator passwordMigrationWarningCoordinator =
                new PasswordMigrationWarningCoordinator(
                        context,
                        profile,
                        bottomSheetController,
                        SyncConsentActivityLauncherImpl.get(),
                        ManageSyncSettings.class,
                        new ExportFlow(),
                        (PasswordListObserver observer) ->
                                PasswordManagerHandlerProvider.getForProfile(profile)
                                        .addObserver(observer),
                        new PasswordStoreBridge(profile),
                        referrer,
                        ChromePureJavaExceptionReporter::reportJavaException);
        passwordMigrationWarningCoordinator.showWarning();
    }

    @CalledByNative
    static void maybeShowPostMigrationSheet(WindowAndroid windowAndroid, Profile profile) {
        PostPasswordMigrationSheetCoordinator postMigrationSheet =
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(
                                windowAndroid, profile);
        if (postMigrationSheet == null) return;
        postMigrationSheet.showSheet();
    }
}
