// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import androidx.annotation.VisibleForTesting;

/**
 * Coordinator of the autofill IBAN save UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a IBAN.
 */
public class AutofillSaveIbanBottomSheetCoordinator {
    private final AutofillSaveIbanBottomSheetMediator mMediator;

    /**
     * Creates the coordinator.
     *
     * @param bridge The bridge to signal UI flow events (OnUiCanceled, OnUiAccepted, etc.) to.
     */
    public AutofillSaveIbanBottomSheetCoordinator(AutofillSaveIbanBottomSheetBridge bridge) {
        mMediator = new AutofillSaveIbanBottomSheetMediator(bridge);
    }

    @VisibleForTesting
    /*package*/ AutofillSaveIbanBottomSheetCoordinator(
            AutofillSaveIbanBottomSheetMediator mediator) {
        mMediator = mediator;
    }

    /**
     * Request to show the bottom sheet.
     *
     * @param ibanLabel String value of the IBAN being shown, i.e. CH56 0483 5012 3456 7800 9.
     */
    public void requestShowContent(String ibanLabel) {
        if (ibanLabel == null || ibanLabel.isEmpty()) {
            throw new IllegalArgumentException(
                    "IBAN label passed from C++ should not be NULL or empty.");
        }
        mMediator.requestShowContent(ibanLabel);
    }

    /** Destroys this component, hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.destroy();
    }
}
