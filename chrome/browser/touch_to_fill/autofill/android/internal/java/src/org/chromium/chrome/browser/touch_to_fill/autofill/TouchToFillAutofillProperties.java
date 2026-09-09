// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties defined here reflect the visible state of the TouchToFillAutofill component. */
@NullMarked
final class TouchToFillAutofillProperties {
    @IntDef({ItemType.HEADER, ItemType.FILL_BUTTON, ItemType.TEXT_BUTTON})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int HEADER = 0;
        int FILL_BUTTON = 1;
        int TEXT_BUTTON = 2;
    }

    static final ReadableObjectPropertyKey<ModelList> SHEET_ITEMS =
            new ReadableObjectPropertyKey<>("sheet_items");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final ReadableObjectPropertyKey<Runnable> DISMISS_HANDLER =
            new ReadableObjectPropertyKey<>("dismiss_handler");

    static final PropertyKey[] ALL_KEYS = {VISIBLE, SHEET_ITEMS, DISMISS_HANDLER};

    private TouchToFillAutofillProperties() {}
}
