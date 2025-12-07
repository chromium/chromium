// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Utility class for tab persistence. */
@NullMarked
public class TabPersistenceUtils {
    /**
     * Returns true if the tab should be skipped when persisting tabs.
     *
     * @param tab The tab to check.
     * @return True if the tab should be skipped, false otherwise.
     */
    public static boolean shouldSkipTab(Tab tab) {
        // Don't skip the tab if it is pinned.
        if (tab.getIsPinned()) return false;

        boolean isNtp = tab.isNativePage() && UrlUtilities.isNtpUrl(tab.getUrl());
        if (!isNtp) return false;

        // Only skip NTP tabs that are not in a tab group.
        return tab.getTabGroupId() == null;
    }

    private TabPersistenceUtils() {}
}
