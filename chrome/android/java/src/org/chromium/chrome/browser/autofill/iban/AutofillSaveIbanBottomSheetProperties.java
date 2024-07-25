// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/*package*/ class AutofillSaveIbanBottomSheetProperties {
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> IBAN_LABEL = new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<OnClickListener> ON_ACCEPT_BUTTON_CLICK_ACTION =
            new ReadableObjectPropertyKey<>();

    static final ReadableObjectPropertyKey<OnClickListener> ON_CANCEL_BUTTON_CLICK_ACTION =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        TITLE,
        IBAN_LABEL,
        ACCEPT_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        ON_ACCEPT_BUTTON_CLICK_ACTION,
        ON_CANCEL_BUTTON_CLICK_ACTION,
    };

    /** Do not instantiate. */
    private AutofillSaveIbanBottomSheetProperties() {}
}
