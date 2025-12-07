// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for price history bottom sheet content. */
@NullMarked
public class PriceHistoryBottomSheetContentProperties {
    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_TITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey PRICE_HISTORY_DESCRIPTION_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<View> PRICE_HISTORY_CHART =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_CHART_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey OPEN_URL_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            OPEN_URL_BUTTON_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PRICE_HISTORY_TITLE,
                PRICE_HISTORY_DESCRIPTION,
                PRICE_HISTORY_DESCRIPTION_VISIBLE,
                PRICE_HISTORY_CHART,
                PRICE_HISTORY_CHART_CONTENT_DESCRIPTION,
                OPEN_URL_BUTTON_VISIBLE,
                OPEN_URL_BUTTON_ON_CLICK_LISTENER
            };
}
