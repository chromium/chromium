// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * Handles the ungrouping of tabs for {@link TabGroupModelFilter}.
 *
 * <p>This interface, combined with {@link TabRemover}, facilitates a shared implementation with the
 * ability to show warning dialogs when events may be destructive to tab groups.
 */
public interface TabUngrouper {
    /**
     * Ungroups one or more tabs from a tab group.
     *
     * @param tabs The list of tabs to move out of their current group.
     * @param trailing False (leading) with put the tabs before the group, true (trailing) will
     *     place the tabs after the group.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void ungroupTabs(
            @NonNull List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * {@link #ungroupTabs(List<Tab>, boolean, boolean, TabModelActionListener)} without the {@code
     * listener}
     */
    default void ungroupTabs(@NonNull List<Tab> tabs, boolean trailing, boolean allowDialog) {
        ungroupTabs(tabs, trailing, allowDialog, /* listener= */ null);
    }

    /**
     * Ungroups an entire tab group.
     *
     * @param rootId The root ID of the tab group to ungroup.
     * @param trailing False (leading) with put the tabs before the group, true (trailing) will
     *     place the tabs after the group.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void ungroupTabGroup(
            int rootId,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * Ungroups an entire tab group.
     *
     * @param tabGroupId The tab group ID of the tab group to ungroup.
     * @param trailing False (leading) with put the tabs before the group, true (trailing) will
     *     place the tabs after the group.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void ungroupTabGroup(
            @NonNull Token tabGroupId,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * {@link #ungroupTabGroup(int, boolean, boolean, TabModelActionListener)} without the {@code
     * listener}
     */
    default void ungroupTabs(int rootId, boolean trailing, boolean allowDialog) {
        ungroupTabGroup(rootId, trailing, allowDialog, /* listener= */ null);
    }

    /**
     * {@link #ungroupTabGroup(Token, boolean, boolean, TabModelActionListener)} without the {@code
     * listener}
     */
    default void ungroupTabs(@NonNull Token tabGroupId, boolean trailing, boolean allowDialog) {
        ungroupTabGroup(tabGroupId, trailing, allowDialog, /* listener= */ null);
    }
}
