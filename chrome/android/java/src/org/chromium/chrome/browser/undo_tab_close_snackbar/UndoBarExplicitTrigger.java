// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Allows for explicitly triggering the undo bar for closures through an action call rather than a
 * passive observer event.
 */
@NullMarked
public interface UndoBarExplicitTrigger {
    /**
     * Trigger the snackbar for a single {@link SavedTabGroup} closure.
     *
     * @param syncId The sync id for the corresponding closed tab group.
     */
    void triggerSnackbarForSavedTabGroup(String syncId);

    /**
     * Trigger the snackbar for a single {@link Tab} closure.
     *
     * @param tab The Tab to be closed.
     */
    void triggerSnackbarForTab(Tab tab);
}
