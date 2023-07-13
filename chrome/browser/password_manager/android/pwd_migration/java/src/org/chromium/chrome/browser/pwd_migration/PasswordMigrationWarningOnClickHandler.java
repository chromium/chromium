// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.net.Uri;

import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Contains the logic for the on click listeners of all the buttons on the
 * password migration warning sheet.
 */
interface PasswordMigrationWarningOnClickHandler {
    /**
     * Closes the sheet and marks that the user acknowledged the notice by
     * clicking on the "Got it" button.
     *
     * @param bottomSheetController used to close the sheet.
     */
    void onAcknowledge(BottomSheetController bottomSheetController);

    /**
     * Shows a screen with more options when the "More options" button is
     * clicked.
     */
    void onMoreOptions();

    /**
     * Starts the sign-in/sync flow or the export flow depending on the user
     * choice in the screen with more options.
     *
     * @param selectedOption is the flow that the user wants to proceed with.
     * @param fragmentManager is the fragment manager of the fragment that offers the options. It's
     *         used for creating the {@link ExportFlow}.
     */
    void onNext(@MigrationOption int selectedOption, FragmentManager fragmentManager);

    /**
     * Triggered when Chrome receives the file created in the file system after user's decision to
     * export the passwords.
     *
     * @param passwordsFile The newly created empty file, into which passwords will be written.
     */
    void onSavePasswordsToDownloads(Uri passwordsFile);

    /**
     * Closes the sheet when the "Cancel" button is clicked, but doesn't mark
     * that the user acknowledged the notice.
     *
     * @param bottomSheetController used to close the sheet.
     */
    void onCancel(BottomSheetController bottomSheetController);
}
