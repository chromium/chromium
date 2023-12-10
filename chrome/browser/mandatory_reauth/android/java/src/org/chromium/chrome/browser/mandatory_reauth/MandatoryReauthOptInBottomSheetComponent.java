// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import org.chromium.components.autofill.PaymentsBubbleClosedReason;

/** This component allows showing the Mandatory Reauth opt-in prompt in a bottom sheet. */
interface MandatoryReauthOptInBottomSheetComponent {
    /** The delegate is used to relay the bottom sheet events to the native side. */
    interface Delegate {
        /** Called when the prompt is closed. */
        void onClosed(@PaymentsBubbleClosedReason int closedReason);
    }

    /** Shows the bottom sheet. */
    boolean show();

    /** Closes the bottom sheet. */
    void close(@PaymentsBubbleClosedReason int closedReason);
}
