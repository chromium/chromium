// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/*package*/ class AutofillSaveIbanBottomSheetProperties {
    static final ReadableObjectPropertyKey<String> IBAN_LABEL = new ReadableObjectPropertyKey();

    static final PropertyKey[] ALL_KEYS = {IBAN_LABEL};

    /** Do not instantiate. */
    private AutofillSaveIbanBottomSheetProperties() {}
}
