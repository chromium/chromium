// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;

import java.util.Arrays;
import java.util.List;

/**
 * This class serves as a callback from TabModel to TabModelSelector. Avoid adding unnecessary
 * methods that expose too much access to TabModel. http://crbug.com/40326216
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

    /** Returns whether reparenting is currently in progress for this TabModel. */
    boolean isReparentingInProgress();

    /**
     * Request to the native TabRestoreService to restore the most recently closed tab.
     *
     * @param model The model requesting the restore.
     */
    default void openMostRecentlyClosedEntry(TabModel model) {}

    /**
     * Requests the native TabRestoreService to restore the tabs of a Recent Tabs entry for an undo
     * operation. The entry is consumed. The delegate itself does not commit pending closures, but
     * adding the restored tabs to {@code tabModel} may commit them.
     *
     * @param tabModel The model to restore the tabs into.
     * @param entryId The id of the TabRestoreService entry holding the closed tabs.
     * @param tabs The closed tabs to restore, each with the placement it had before closure.
     * @return The restored tab ids using the same indexing as {@code tabs}, i.e. element {@code i}
     *     is the new id of {@code tabs.get(i)}, or {@link Tab#INVALID_TAB_ID} if that tab was not
     *     restored. Always has {@code tabs.size()} elements.
     */
    default int[] restoreForUndo(
            TabModel tabModel, @TabRestoreEntryId int entryId, List<ClosedTab> tabs) {
        int[] restoredIds = new int[tabs.size()];
        Arrays.fill(restoredIds, Tab.INVALID_TAB_ID);
        return restoredIds;
    }

    /**
     * Gets the timestamp of the most recent tab closure event stored by the native
     * TabRestoreService. If a valid, non-zero timestamp is not available, this should return {@link
     * TabModel#INVALID_TIMESTAMP}.
     *
     * @return The closure timestamp, in millis.
     */
    default long getMostRecentClosureTime() {
        return TabModel.INVALID_TIMESTAMP;
    }

    // TODO(aurimas): clean these methods up.
    TabModel getCurrentModel();

    /** Provides the top level tab manager object for the current scope. */
    TabModel getModel(boolean incognito);

    /**
     * Whether all the tabs in the tab model have been restored from disk. If this is false session
     * restore is still ongoing.
     */
    boolean isTabStateInitialized();

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
     * @return Whether the move succeeded.
     */
    default boolean moveTabGroupToWindow(
            Token tabGroupId, Activity activity, int newIndex, boolean isIncognito) {
        return false;
    }
}
