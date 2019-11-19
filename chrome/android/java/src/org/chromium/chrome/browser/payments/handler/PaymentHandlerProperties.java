// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;

/** PaymentHandler UI properties, which fully describe the state of the UI. */
/* package */ class PaymentHandlerProperties {
    /** The height fraction defined in {@link BottomSHeetObserver#onSheetOffsetChanged} */
    /* package */ static final WritableFloatPropertyKey BOTTOM_SHEET_HEIGHT_FRACTION =
            new WritableFloatPropertyKey();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {BOTTOM_SHEET_HEIGHT_FRACTION};

    // Prevent instantiation.
    private PaymentHandlerProperties() {}
}
