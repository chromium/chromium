// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for price tracking bottom sheet content. */
@NullMarked
public class PriceTrackingBottomSheetContentProperties {

    public static final WritableObjectPropertyKey<String> PRICE_TRACKING_TITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_TRACKING_SUBTITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRICE_TRACKING_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_ICON =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_FOREGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey PRICE_TRACKING_BUTTON_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PRICE_TRACKING_TITLE,
                PRICE_TRACKING_SUBTITLE,
                PRICE_TRACKING_BUTTON_TEXT,
                PRICE_TRACKING_BUTTON_ICON,
                PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER
            };
}
