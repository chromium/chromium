// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

/**
 * Creates and holds a unique instance of a {@link PasswordCsvDownloadFlowController} for as long as
 * the download flow is ongoing.
 */
public class PasswordCsvDownloadFlowControllerFactory {
    private static PasswordCsvDownloadFlowController sController;

    private PasswordCsvDownloadFlowControllerFactory() {}

    public static PasswordCsvDownloadFlowController getOrCreateController() {
        if (sController == null) {
            sController =
                    new PasswordCsvDownloadFlowController(
                            PasswordCsvDownloadFlowControllerFactory::releaseController);
        }
        return sController;
    }

    private static void releaseController() {
        sController = null;
    }
}
