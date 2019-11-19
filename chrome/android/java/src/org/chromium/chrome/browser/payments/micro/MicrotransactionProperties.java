// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Microtransaction UI properties, which fully describe the state of the UI. */
/* package */ class MicrotransactionProperties {
    /* package */ static final ReadableObjectPropertyKey<Drawable> PAYMENT_APP_ICON =
            new ReadableObjectPropertyKey<>();

    /* package */ static final ReadableObjectPropertyKey<CharSequence> AMOUNT =
            new ReadableObjectPropertyKey<>();

    /* package */ static final ReadableObjectPropertyKey<CharSequence> CURRENCY =
            new ReadableObjectPropertyKey<>();

    /* package */ static final ReadableObjectPropertyKey<CharSequence> PAYMENT_APP_NAME =
            new ReadableObjectPropertyKey<>();

    /* package */ static final WritableBooleanPropertyKey IS_PEEK_STATE_ENABLED =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableBooleanPropertyKey IS_SHOWING_PROCESSING_SPINNER =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableBooleanPropertyKey IS_SHOWING_PAY_BUTTON =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableBooleanPropertyKey IS_STATUS_EMPHASIZED =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableBooleanPropertyKey IS_SHOWING_LINE_ITEMS =
            new WritableBooleanPropertyKey();

    /* package */ static final WritableFloatPropertyKey PAYMENT_APP_NAME_ALPHA =
            new WritableFloatPropertyKey();

    /* package */ static final WritableObjectPropertyKey<Integer> STATUS_ICON =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<Integer> STATUS_ICON_TINT =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<Integer> STATUS_TEXT_RESOURCE =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<CharSequence> ACCOUNT_BALANCE =
            new WritableObjectPropertyKey<>();

    /* package */ static final WritableObjectPropertyKey<CharSequence> STATUS_TEXT =
            new WritableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PAYMENT_APP_ICON, AMOUNT,
            CURRENCY, PAYMENT_APP_NAME, IS_PEEK_STATE_ENABLED, IS_SHOWING_PROCESSING_SPINNER,
            IS_SHOWING_PAY_BUTTON, IS_STATUS_EMPHASIZED, IS_SHOWING_LINE_ITEMS,
            PAYMENT_APP_NAME_ALPHA, STATUS_ICON, STATUS_ICON_TINT, STATUS_TEXT_RESOURCE,
            ACCOUNT_BALANCE, STATUS_TEXT};

    // Prevent instantiation.
    private MicrotransactionProperties() {}
}
