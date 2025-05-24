// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** The factory used to create the coordinator of the post password migration sheet. */
@NullMarked
public class PostPasswordMigrationSheetCoordinatorFactory {

    private static @Nullable PostPasswordMigrationSheetCoordinator sCoordinatorInstanceForTesting;

    /**
     * Creates and returns a new {@link PostPasswordMigrationSheetCoordinator} or the instance set
     * for testing.
     *
     * @param windowAndroid is used to get the {@link BottomSheetController}.
     * @return {@link PostPasswordMigrationSheetCoordinator} or null.
     */
    public static @Nullable
            PostPasswordMigrationSheetCoordinator
                    maybeGetOrCreatePostPasswordMigrationSheetCoordinator(
                            WindowAndroid windowAndroid, Profile profile) {
        if (sCoordinatorInstanceForTesting != null) {
            return sCoordinatorInstanceForTesting;
        }
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }
        return new PostPasswordMigrationSheetCoordinator(context, bottomSheetController, profile);
    }

    public static void setCoordinatorInstanceForTesting(
            PostPasswordMigrationSheetCoordinator testingCoordinator) {
        sCoordinatorInstanceForTesting = testingCoordinator;
        ResettersForTesting.register(() -> sCoordinatorInstanceForTesting = null);
    }

    private PostPasswordMigrationSheetCoordinatorFactory() {}
}
