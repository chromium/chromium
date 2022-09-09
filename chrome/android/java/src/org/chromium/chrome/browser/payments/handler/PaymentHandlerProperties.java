// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** PaymentHandler UI properties, which fully describe the state of the UI. */
/* package */ class PaymentHandlerProperties {
    /** The visible height of the PaymentHandler UI's content area in pixels. */
    /* package */ static final WritableIntPropertyKey CONTENT_VISIBLE_HEIGHT_PX =
            new WritableIntPropertyKey();

    /** The callback when the system back button is pressed. */
    /* package */ static final WritableObjectPropertyKey<Runnable> BACK_PRESS_CALLBACK =
            new WritableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CONTENT_VISIBLE_HEIGHT_PX, BACK_PRESS_CALLBACK};

    // Prevent instantiation.
    private PaymentHandlerProperties() {}
}
