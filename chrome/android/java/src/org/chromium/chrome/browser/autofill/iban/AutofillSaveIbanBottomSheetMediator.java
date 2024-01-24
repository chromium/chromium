// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import java.util.function.Consumer;

/**
 * Mediator class for the autofill IBAN save UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a IBAN.
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiCanceled, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class AutofillSaveIbanBottomSheetMediator {
    private final AutofillSaveIbanBottomSheetBridge mBridge;
    private boolean mFinished;

    /**
     * Creates the mediator.
     *
     * @param bridge The bridge to signal UI flow events (OnUiCanceled, OnUiAccepted, etc.) to.
     */
    AutofillSaveIbanBottomSheetMediator(AutofillSaveIbanBottomSheetBridge bridge) {
        mBridge = bridge;
    }

    /**
     * Requests to show the bottom sheet content.
     *
     * @param ibanLabel String value of the IBAN being saved, i.e. CH56 0483 5012 3456 7800 9.
     */
    void requestShowContent(String ibanLabel) {}

    void destroy() {
        finish(AutofillSaveIbanBottomSheetBridge::onUiIgnored);
    }

    private void finish(Consumer<AutofillSaveIbanBottomSheetBridge> bridgeCallback) {
        if (!mFinished) {
            mFinished = true;
            bridgeCallback.accept(mBridge);
        }
    }
}
