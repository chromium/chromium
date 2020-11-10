// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Delegate for Chrome startup initialization, implemented downstream.
 */
public class ChromeStartupDelegate {
    public static final String ENABLED_PARAM = "enabled";
    public static final BooleanCachedFieldTrialParameter ENABLED =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.CHROME_STARTUP_DELEGATE, ENABLED_PARAM, false);

    public void init() {}
}
