// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/** Utility class for Chrome drag and drop implementations. */
public class ChromeDragDropUtils {

    /**
     * @return {@code true} if tab tearing with the intent of opening a new window should be
     *     allowed, {@code false} otherwise.
     */
    public static boolean shouldAllowTabTearing(TabModelSelector tabModelSelector) {
        if (!TabUiFeatureUtilities.isTabTearingEnabled() || tabModelSelector == null) return false;

        // Allow tearing a tab with an intent of opening a new window, only if it is not the only
        // tab in the window. This is to avoid creating a new window from such a tab.
        return tabModelSelector.getTotalTabCount() > 1;
    }
}
