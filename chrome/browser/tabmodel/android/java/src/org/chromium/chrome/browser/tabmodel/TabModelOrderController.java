// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;

/**
 * This class acts as a controller for determining where tabs should be inserted into a tab strip
 * model.
 *
 * <p>TODO(crbug.com/40152902): Move to chrome/browser/tabmodel/internal when all usages are
 * modularized.
 */
@NullMarked
interface TabModelOrderController {
    /**
     * Determines the insertion index of the next tab. Returns {@link TabList#INVALID_TAB_INDEX} to
     * let the {@link TabModel} choose the index when {@code type} bypasses insertion order
     * calculation, or when a pinned {@code newTab} cannot be placed at {@code position}. Otherwise
     * returns {@code position}, unless the launch type places the tab at the end of the model or
     * adjacent to its opener.
     *
     * @param type The launch type of the new tab.
     * @param position The provided position.
     * @param newTab The new tab to insert. Its pinned state and parent can affect the index.
     * @return Where to insert the tab.
     */
    int determineInsertionIndex(@TabLaunchType int type, int position, Tab newTab);

    /**
     * Determine if a launch type will result in the tab being opened in the foreground.
     *
     * @param type The type of opening event.
     * @param isNewTabIncognitoBranded True if the new opened tab is incognito.
     * @return True if the tab will be in the foreground
     */
    boolean willOpenInForeground(@TabLaunchType int type, boolean isNewTabIncognitoBranded);
}
