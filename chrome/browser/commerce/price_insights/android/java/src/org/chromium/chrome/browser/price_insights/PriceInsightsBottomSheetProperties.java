// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for price insights bottom sheet. */
public class PriceInsightsBottomSheetProperties {
    public static final WritableObjectPropertyKey<String> PRICE_TRACKING_TITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_TRACKING_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_ICON =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_FOREGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey PRICE_TRACKING_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_TITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_HISTORY_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View> PRICE_HISTORY_CHART =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey OPEN_URL_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            OPEN_URL_BUTTON_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PRICE_TRACKING_TITLE,
                PRICE_TRACKING_BUTTON_TEXT,
                PRICE_TRACKING_BUTTON_ICON,
                PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                PRICE_TRACKING_BUTTON_ENABLED,
                PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER,
                PRICE_HISTORY_TITLE,
                PRICE_HISTORY_DESCRIPTION,
                PRICE_HISTORY_CHART,
                OPEN_URL_BUTTON_VISIBLE,
                OPEN_URL_BUTTON_ON_CLICK_LISTENER
            };
}
