// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetProperties.DISMISS_HANDLER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Autofill AI attribution verification bottom sheet. */
@NullMarked
/*package*/ class AutofillAiAttributionVerificationSheetViewBinder {
    static void bind(
            PropertyModel model,
            AutofillAiAttributionVerificationSheetView view,
            PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private AutofillAiAttributionVerificationSheetViewBinder() {}
}
