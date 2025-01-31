// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinator;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinatorFactory;
import org.chromium.ui.base.WindowAndroid;

/** The bridge that is used to show the password migration warning. */
class PasswordMigrationWarningBridge {
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
