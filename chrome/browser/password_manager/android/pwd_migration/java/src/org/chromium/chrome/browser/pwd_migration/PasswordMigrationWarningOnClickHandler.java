// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

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
     * Closes the sheet when the "Cancel" button is clicked, but doesn't mark
     * that the user acknowledged the notice.
     *
     * @param bottomSheetController used to close the sheet.
     */
    void onCancel(BottomSheetController bottomSheetController);
}
