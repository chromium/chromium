// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

/** PaymentHandlerToolbar UI properties, which fully describe the state of the UI. */
/* package */ class PaymentHandlerToolbarProperties {
    /* package */ static final WritableObjectPropertyKey<GURL> URL =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableFloatPropertyKey LOAD_PROGRESS =
            new WritableFloatPropertyKey();

    /* package */ static final WritableBooleanPropertyKey PROGRESS_VISIBLE =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableIntPropertyKey SECURITY_ICON = new WritableIntPropertyKey();

    /* package */ static final WritableObjectPropertyKey<String> SECURITY_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The callback when the security icon is clicked. */
    /* package */ static final WritableObjectPropertyKey<Runnable> SECURITY_ICON_ON_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The callback when the close icon is clicked. */
    /* package */ static final WritableObjectPropertyKey<Runnable> CLOSE_BUTTON_ON_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                URL,
                TITLE,
                LOAD_PROGRESS,
                PROGRESS_VISIBLE,
                SECURITY_ICON,
                SECURITY_ICON_CONTENT_DESCRIPTION,
                SECURITY_ICON_ON_CLICK_CALLBACK,
                CLOSE_BUTTON_ON_CLICK_CALLBACK
            };

    // Prevent instantiation.
    private PaymentHandlerToolbarProperties() {}
}
