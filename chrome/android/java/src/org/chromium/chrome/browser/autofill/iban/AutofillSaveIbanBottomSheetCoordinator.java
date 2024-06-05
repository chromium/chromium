// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.content.Context;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

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
     * @param context The context for this component.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     */
    public AutofillSaveIbanBottomSheetCoordinator(
            AutofillSaveIbanBottomSheetBridge bridge,
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel) {
        mMediator =
                new AutofillSaveIbanBottomSheetMediator(
                        bridge,
                        new AutofillSaveIbanBottomSheetContent(context),
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel);
    }

    /**
     * Request to show the bottom sheet.
     *
     * @param ibanLabel String value of the IBAN being shown, e.g. CH56 **** **** **** *800 9.
     */
    void requestShowContent(String ibanLabel) {
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
