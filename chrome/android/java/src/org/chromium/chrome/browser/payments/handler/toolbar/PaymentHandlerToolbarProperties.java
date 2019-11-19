// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.net.URI;

/** PaymentHandlerToolbar UI properties, which fully describe the state of the UI. */
/* package */ class PaymentHandlerToolbarProperties {
    /* package */ static final WritableObjectPropertyKey<URI> ORIGIN =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableFloatPropertyKey LOAD_PROGRESS =
            new WritableFloatPropertyKey();

    /* package */ static final WritableBooleanPropertyKey PROGRESS_VISIBLE =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableIntPropertyKey SECURITY_ICON = new WritableIntPropertyKey();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ORIGIN, TITLE, LOAD_PROGRESS, PROGRESS_VISIBLE, SECURITY_ICON};

    // Prevent instantiation.
    private PaymentHandlerToolbarProperties() {}
}
