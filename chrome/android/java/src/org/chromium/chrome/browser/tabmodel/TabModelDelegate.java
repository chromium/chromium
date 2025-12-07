// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * This class serves as a callback from TabModel to TabModelSelector. Avoid adding unnecessary
 * methods that expose too much access to TabModel. http://crbug.com/263579
 */
@NullMarked
public interface TabModelDelegate {
    /**
     * Requests the specified to be shown.
     *
     * @param tab The tab that is requested to be shown.
     * @param type The reason why this tab was requested to be shown.
     */
    void requestToShowTab(@Nullable Tab tab, @TabSelectionType int type);

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

    /** Provides the top level tab manager object for the current scope. */
    TabModel getModel(boolean incognito);

    /** Provides the top level tab group manager object for the current scope. */
    TabGroupModelFilter getFilter(boolean incognito);

    /**
     * Whether all the tabs in the tab model have been restored from disk. If this is false session
     * restore is still ongoing.
     */
    boolean isTabModelRestored();

    void selectModel(boolean incognito);

    /**
     * Moves a tab to a new window.
     *
     * @param tab The tab to move.
     * @param activity The activity to move the tab to.
     * @param newIndex The index to move the tab to.
     */
    default void moveTabToWindow(Tab tab, Activity activity, int newIndex) {}

    /**
     * Moves a tab group to a new window.
     *
     * @param tabGroupId The tab group to move.
     * @param activity The activity to move the tab group to.
     * @param newIndex The index to move the tab group to.
     * @param isIncognito Whether the tab group is in the incognito model.
     */
    default void moveTabGroupToWindow(
            Token tabGroupId, Activity activity, int newIndex, boolean isIncognito) {}
}
