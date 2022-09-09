// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the TouchToFillCreditCard component. It sets the state of the model
 * and reacts to events like clicks.
 */
class TouchToFillCreditCardMediator {
    private TouchToFillCreditCardComponent.Delegate mDelegate;
    private PropertyModel mModel;

    void initialize(TouchToFillCreditCardComponent.Delegate delegate, PropertyModel model) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
    }

    void showSheet() {
        mModel.set(VISIBLE, true);
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
