// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Merchant Viewer experience. */
public class MerchantViewerConfig {
    private static final String TRUST_SIGNALS_MESSAGE_DELAY_PARAM =
            "trust_signals_message_delay_ms";
    private static final String TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM =
            "trust_signals_message_window_duration_ms";
    private static final String TRUST_SIGNALS_SHEET_USE_PAGE_TITLE_PARAM =
            "trust_signals_sheet_use_page_title";
    private static final String TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM =
            "trust_signals_message_use_rating_bar";

    public static final IntCachedFieldTrialParameter DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY =
            new IntCachedFieldTrialParameter(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DELAY_PARAM, (int) TimeUnit.SECONDS.toMillis(30));

    public static final IntCachedFieldTrialParameter TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_SECONDS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM, (int) TimeUnit.DAYS.toMillis(365));

    public static final BooleanCachedFieldTrialParameter TRUST_SIGNALS_SHEET_USE_PAGE_TITLE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_SHEET_USE_PAGE_TITLE_PARAM, true);

    public static final BooleanCachedFieldTrialParameter TRUST_SIGNALS_MESSAGE_USE_RATING_BAR =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM, true);
}
