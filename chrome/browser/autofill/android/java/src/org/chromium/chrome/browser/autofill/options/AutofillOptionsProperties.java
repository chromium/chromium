// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.text.SpannableString;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Collection of properties that affect the autofill options settings screen. */
class AutofillOptionsProperties {
    static final WritableBooleanPropertyKey THIRD_PARTY_AUTOFILL_ENABLED =
            new WritableBooleanPropertyKey("third_party_autofill_enabled");
    static final WritableBooleanPropertyKey THIRD_PARTY_TOGGLE_IS_READ_ONLY =
            new WritableBooleanPropertyKey("third_party_toggle_is_read_only");
    static final ReadableObjectPropertyKey<Callback<Boolean>> ON_THIRD_PARTY_TOGGLE_CHANGED =
            new ReadableObjectPropertyKey<>("on_third_party_toggle_changed");
    static final WritableObjectPropertyKey<SpannableString> THIRD_PARTY_TOGGLE_HINT =
            new WritableObjectPropertyKey<>("third_party_toggle_hint");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                THIRD_PARTY_AUTOFILL_ENABLED,
                THIRD_PARTY_TOGGLE_IS_READ_ONLY,
                ON_THIRD_PARTY_TOGGLE_CHANGED,
                THIRD_PARTY_TOGGLE_HINT,
            };

    /**
     * Never instantiate this class. Create the model with {@code PropertyModel.Builder(ALL_KEYS)}.
     */
    private AutofillOptionsProperties() {}
}
