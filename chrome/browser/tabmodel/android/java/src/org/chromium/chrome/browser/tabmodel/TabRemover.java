// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Handles removal of {@link Tab} entities from a {@link TabModel} via closure or removal. Also
 * handles the ungrouping of tabs for {@link TabGroupModelFilter}. This interface is intended to be
 * a counterpart of {@link TabCreator}.
 *
 * <p>This interface, combined with {@link TabUngrouper}, facilitates a shared implementation with
 * the ability to show warning dialogs when events may be destructive to tab groups.
 */
public interface TabRemover {
    /**
     * Closes tabs based on the provided parameters. Refer to {@link TabClosureParams} for different
     * ways to close tabs.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void closeTabs(
            @NonNull TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * {@link #closeTabs(TabClosureParams, boolean, TabModelActionListener)} without the {@code
     * listener}.
     */
    default void closeTabs(@NonNull TabClosureParams tabClosureParams, boolean allowDialog) {
        closeTabs(tabClosureParams, allowDialog, /* listener= */ null);
    }

    /**
     * Closes tabs bypassing any dialogs and data sharing protections. This should only be used by
     * {@link TabGroupSyncService} to forcibly close tab groups due to sync updates.
     */
    void forceCloseTabs(@NonNull TabClosureParams tabClosureParams);

    /**
     * Removes the given tab from the model without destroying it. The tab should be inserted into
     * another model to avoid leaking as after this the link to the old Activity will be broken.
     *
     * @param tab The tab to remove.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void removeTab(
            @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener);

    /** {@link #removeTab(Tab, boolean, TabModelActionListener)} without the {@code listener}. */
    default void removeTab(@NonNull Tab tab, boolean allowDialog) {
        removeTab(tab, allowDialog, /* listener= */ null);
    }
}
