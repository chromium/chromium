// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * This class serves as a callback from TabModel to TabModelSelector. Avoid adding unnecessary
 * methods that expose too much access to TabModel. http://crbug.com/263579
 */
public interface TabModelDelegate {
    /**
     * Requests the specified to be shown.
     * @param tab The tab that is requested to be shown.
     * @param type The reason why this tab was requested to be shown.
     */
    void requestToShowTab(Tab tab, @TabSelectionType int type);

    /**
     * @return Whether reparenting is currently in progress for this TabModel.
     */
    boolean isReparentingInProgress();

    /**
     * Request to the native TabRestoreService to restore the most recently closed tab.
     *
     * @param model The model requesting the restore.
     */
    default void openMostRecentlyClosedEntry(TabModel model) {}

    // TODO(aurimas): clean these methods up.
    TabModel getCurrentModel();

    TabModel getModel(boolean incognito);

    boolean isSessionRestoreInProgress();

    void selectModel(boolean incognito);
}
