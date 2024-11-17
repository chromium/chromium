// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

/**
 * Common tab UI feature utils for public use. TODO(crbug.com/40825348) Move other @{@link
 * TabUiFeatureUtilities} methods that are required by chrome/browser. TabUiFeatureUtilities}
 * methods that are required by chrome/browser.
 */
public class TabManagementFieldTrial {
    private static final String DELAY_TEMP_STRIP_REMOVAL_TIMEOUT_MS_PARAM = "timeout_ms";
    public static final IntCachedFieldTrialParameter DELAY_TEMP_STRIP_TIMEOUT_MS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL,
                    DELAY_TEMP_STRIP_REMOVAL_TIMEOUT_MS_PARAM,
                    1000);
}
