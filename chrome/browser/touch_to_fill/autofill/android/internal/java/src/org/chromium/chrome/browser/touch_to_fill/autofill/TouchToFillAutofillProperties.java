// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties defined here reflect the visible state of the TouchToFillAutofill component. */
@NullMarked
final class TouchToFillAutofillProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final ReadableObjectPropertyKey<Runnable> ACKNOWLEDGE_HANDLER =
            new ReadableObjectPropertyKey<>("acknowledge_handler");
    static final ReadableObjectPropertyKey<Runnable> SETTINGS_LINK_HANDLER =
            new ReadableObjectPropertyKey<>("settings_link_handler");
    static final ReadableObjectPropertyKey<Runnable> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, ACKNOWLEDGE_HANDLER, SETTINGS_LINK_HANDLER, DISMISS_HANDLER
    };

    private TouchToFillAutofillProperties() {}
}
