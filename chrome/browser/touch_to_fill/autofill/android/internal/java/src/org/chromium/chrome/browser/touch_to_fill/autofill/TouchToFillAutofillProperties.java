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
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

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

    /** Announced by screen readers when the sheet is opened at full height. */
    static final WritableIntPropertyKey SHEET_FULL_HEIGHT_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_full_height_description_id");

    /** Announced by screen readers when the sheet is closed. */
    static final WritableIntPropertyKey SHEET_CLOSED_DESCRIPTION_ID =
            new WritableIntPropertyKey("sheet_closed_description_id");

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
        SHEET_ITEMS,
        DISMISS_HANDLER,
        SHEET_FULL_HEIGHT_DESCRIPTION_ID,
        SHEET_CLOSED_DESCRIPTION_ID
    };

    private TouchToFillAutofillProperties() {}
}
