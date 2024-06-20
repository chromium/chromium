// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for price insights bottom sheet. */
public class PriceInsightsBottomSheetProperties {
    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_TITLE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PRICE_HISTORY_TITLE};
}
