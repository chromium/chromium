// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Merchant Viewer experience. */
public class MerchantViewerConfig {
    private static final String TRUST_SIGNALS_MESSAGE_DELAY_PARAM = "trust_signals_message_delay";

    public static final IntCachedFieldTrialParameter DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY =
            new IntCachedFieldTrialParameter(ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DELAY_PARAM, (int) TimeUnit.SECONDS.toMillis(30));
}
