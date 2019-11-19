// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;

/**
 * This class acts as a controller for determining where tabs should be inserted
 * into a tab strip model.
 */
public interface TabModelOrderController {
    /**
     * Determine the insertion index of the next tab. If it's not the result of
     * a link being pressed, the provided index will be returned.
     *
     * @param type The launch type of the new tab.
     * @param position The provided position.
     * @return Where to insert the tab.
     */
    int determineInsertionIndex(@TabLaunchType int type, int position, Tab newTab);

    /**
     * Determine the insertion index of the next tab.
     *
     * @param type The launch type of the new tab.
     * @return Where to insert the tab.
     */
    int determineInsertionIndex(@TabLaunchType int type, Tab newTab);

    /**
     * Determine if a launch type will result in the tab being opened in the
     * foreground.
     * @param type               The type of opening event.
     * @param isNewTabIncognito  True if the new opened tab is incognito.
     * @return                   True if the tab will be in the foreground
     */
    boolean willOpenInForeground(@TabLaunchType int type, boolean isNewTabIncognito);
}
