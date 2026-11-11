// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for the Autofill AI attribution verification bottom sheet. */
@NullMarked
/*package*/ class AutofillAiAttributionVerificationSheetProperties {
    static final ReadableObjectPropertyKey<Runnable> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {DISMISS_HANDLER};

    private AutofillAiAttributionVerificationSheetProperties() {}
}
