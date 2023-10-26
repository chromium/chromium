// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;

/**
 * Common tab UI feature utils for public use.
 * TODO(crbug.com/1302456) Move other @{@link TabUiFeatureUtilities} methods that are required by
 * chrome/browser.
 */
public class TabManagementFieldTrial {
    private static final String DELAY_TEMP_STRIP_REMOVAL_TIMEOUT_MS_PARAM = "timeout_ms";
    public static final IntCachedFieldTrialParameter DELAY_TEMP_STRIP_TIMEOUT_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL,
                    DELAY_TEMP_STRIP_REMOVAL_TIMEOUT_MS_PARAM, 1000);

    // Field trial parameter for enabling folio for tab strip redesign.
    private static final String TAB_STRIP_REDESIGN_ENABLE_FOLIO_PARAM = "enable_folio";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_ENABLE_FOLIO =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_ENABLE_FOLIO_PARAM,
                    true);

    // Field trial parameter for enabling detached for tab strip redesign.
    private static final String TAB_STRIP_REDESIGN_ENABLE_DETACHED_PARAM = "enable_detached";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_ENABLE_DETACHED =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_ENABLE_DETACHED_PARAM, false);

    /**
     * @return Whether Folio for tab strip redesign is enabled.
     */
    public static boolean isTabStripFolioEnabled() {
        return TAB_STRIP_REDESIGN_ENABLE_FOLIO.getValue();
    }

    /**
     * @return Whether Detached for tab strip redesign is enabled.
     */
    public static boolean isTabStripDetachedEnabled() {
        return TAB_STRIP_REDESIGN_ENABLE_DETACHED.getValue();
    }
}
