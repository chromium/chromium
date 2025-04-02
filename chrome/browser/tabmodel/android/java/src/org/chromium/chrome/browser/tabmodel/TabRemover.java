// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Handles removal of {@link Tab} entities from a {@link TabModel} via closure or removal. Also
 * handles the ungrouping of tabs for {@link TabGroupModelFilter}. This interface is intended to be
 * a counterpart of {@link TabCreator}.
 *
 * <p>This interface, combined with {@link TabUngrouper}, facilitates a shared implementation with
 * the ability to show warning dialogs when events may be destructive to tab groups.
 */
@NullMarked
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
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * {@link #closeTabs(TabClosureParams, boolean, TabModelActionListener)} without the {@code
     * listener} or {@code performActionOverride}.
     */
    default void closeTabs(TabClosureParams tabClosureParams, boolean allowDialog) {
        closeTabs(tabClosureParams, allowDialog, /* listener= */ null);
    }

    /**
     * Prepares to close tabs based on the provided parameters. This is similar to {@link
     * closeTabs}. However, it doesn't close the tabs. Instead the final {@link TabClosureParams}
     * are supplied to the {@code onPreparedCallback}. This allows a caller to then perform
     * additional actions before committing to close the tabs with {@link forceCloseTabs} or {@link
     * closeTabs} with {@code allowDialog = false}.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     * @param onPreparedCallback A callback invoked with {@code tabClosureParams} that should be
     *     used to close the tabs.
     */
    void prepareCloseTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onPreparedCallback);

    /** Closes tabs bypassing any dialogs and data sharing protections. */
    void forceCloseTabs(TabClosureParams tabClosureParams);

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
    void removeTab(Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener);

    /** {@link #removeTab(Tab, boolean, TabModelActionListener)} without the {@code listener}. */
    default void removeTab(Tab tab, boolean allowDialog) {
        removeTab(tab, allowDialog, /* listener= */ null);
    }
}
