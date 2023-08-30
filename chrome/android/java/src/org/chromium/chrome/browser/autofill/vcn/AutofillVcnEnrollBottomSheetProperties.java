// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** The model of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ abstract class AutofillVcnEnrollBottomSheetProperties {
    /** The prompt message for the bottom sheet. */
    /*package*/ static final ReadableObjectPropertyKey<String> MESSAGE_TEXT =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that enrolls a virtual card. */
    /*package*/ static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that cancels enrollment. */
    /*package*/ static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /*package*/ static final PropertyKey[] ALL_KEYS = {
            MESSAGE_TEXT, ACCEPT_BUTTON_LABEL, CANCEL_BUTTON_LABEL};
}
