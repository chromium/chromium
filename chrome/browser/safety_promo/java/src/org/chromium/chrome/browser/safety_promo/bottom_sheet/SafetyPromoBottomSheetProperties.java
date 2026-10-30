// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties defined for the Safety Promo "Show me how" bottom sheet. */
@NullMarked
class SafetyPromoBottomSheetProperties {
    /** String resource ID of the sheet title. */
    public static final ReadableIntPropertyKey TITLE_RES_ID = new ReadableIntPropertyKey();

    /** Click listener of the "Got it" button. */
    public static final WritableObjectPropertyKey<@Nullable OnClickListener> ON_GOT_IT_CLICKED =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE_RES_ID, ON_GOT_IT_CLICKED};
}
