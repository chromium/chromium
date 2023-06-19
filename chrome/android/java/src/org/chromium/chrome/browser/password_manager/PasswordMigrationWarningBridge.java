// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningCoordinator;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** The bridge that is used to show the password migration warning. */
class PasswordMigrationWarningBridge {
    @CalledByNative
    static void showWarning(WindowAndroid windowAndroid, Profile profile) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return;
        PasswordMigrationWarningCoordinator passwordMigrationWarningCoordinator =
                new PasswordMigrationWarningCoordinator(windowAndroid.getContext().get(), profile,
                        bottomSheetController, new SettingsLauncherImpl(),
                        ManageSyncSettings.class);
        passwordMigrationWarningCoordinator.showWarning();
    }
}
