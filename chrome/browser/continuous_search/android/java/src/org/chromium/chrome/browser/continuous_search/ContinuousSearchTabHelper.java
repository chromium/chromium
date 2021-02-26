// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Bootstraps continuous search by creating appropriate observers and user data objects.
 */
public class ContinuousSearchTabHelper {
    private ContinuousSearchTabHelper() {}

    /**
     * Activates continuous search navigation support for a tab.
     * @param tab to enable continuous search support for.
     */
    public static void createForTab(Tab tab) {
        if (!FeatureList.isNativeInitialized()) return;

        if (!tab.isIncognito()) new BackNavigationTabObserver(tab);

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTINUOUS_SEARCH)) return;

        new ContinuousSearchTabObserver(tab);
    }
}
