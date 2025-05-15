// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.fragment.app.FragmentActivity;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Creates and holds a unique instance of a {@link PasswordCsvDownloadFlowController} for as long as
 * the download flow is ongoing.
 */
@NullMarked
public class PasswordCsvDownloadFlowControllerFactory {
    private static @Nullable PasswordCsvDownloadFlowController sController;

    /** Testing only */
    public static void setControllerForTesting(PasswordCsvDownloadFlowController controller) {
        sController = controller;
        ResettersForTesting.register(() -> sController = null);
    }

    private PasswordCsvDownloadFlowControllerFactory() {}

    public static PasswordCsvDownloadFlowController getOrCreateController() {
        if (sController == null) {
            sController =
                    new PasswordCsvDownloadFlowController(
                            PasswordCsvDownloadFlowControllerFactory::releaseController);
        }
        return sController;
    }

    /**
     * Re-initializes the component after the activity and fragment have been re-created. This is
     * needed in cases in which the system temporarily destroys the activity hosting the export flow
     * while the file chooser activity is open. Upon coming back to Chrome, the activity and
     * fragment are re-created and they need to be rewired.
     *
     * @param fragmentActivity The newly created activity.
     * @param fragment The newly created fragment.
     */
    static void reinitializeComponent(
            FragmentActivity fragmentActivity, PasswordCsvDownloadDialogFragment fragment) {
        assumeNonNull(sController);
        sController.reinitializeComponent(fragmentActivity, fragment);
    }

    private static void releaseController() {
        sController = null;
    }
}
