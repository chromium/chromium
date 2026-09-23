// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/** Utility methods for querying behavioral traits and predicates of {@link TabLaunchType}. */
@NullMarked
public final class TabLaunchTypeUtils {
    private TabLaunchTypeUtils() {}

    private static void assertValidLaunchType(@TabLaunchType int type) {
        assert type >= 0 && type < TabLaunchType.SIZE : "Invalid TabLaunchType: " + type;
    }

    /**
     * Returns true if the given launch type creates a tab in the background without immediately
     * stealing user focus or activating the tab.
     *
     * @param type The launch type to inspect.
     * @return True if the tab is launched in the background.
     */
    public static boolean isBackgroundLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_RECENT_TABS,
                    TabLaunchType.FROM_SYNC_BACKGROUND,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND,
                    TabLaunchType.FROM_REPARENTING_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND,
                    TabLaunchType.FROM_OMNIBOX_BACKGROUND ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the given launch type corresponds to tab restoration where the active tab
     * selection and indexing is handled externally by session or state restore machinery.
     *
     * <p>Callers (such as {@code TabModelOrderController#willOpenInForeground}) rely on this
     * predicate to ensure that tabs created during restore flows are not automatically selected or
     * treated as newly opened user tabs.
     *
     * <ul>
     *   <li>{@link TabLaunchType#FROM_RESTORE}: Standard session or tab state restoration.
     *   <li>{@link TabLaunchType#FROM_RESTORE_TABS_UI}: Explicit user-initiated tab restore from
     *       UI.
     *   <li>{@link TabLaunchType#FROM_BROWSER_ACTIONS}: External browser action restore where
     *       active tab selection and indexing are managed externally by the caller.
     * </ul>
     *
     * @param type The launch type to inspect.
     * @return True if the launch is a restoration launch.
     */
    public static boolean isRestoreLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_RESTORE,
                    TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabLaunchType.FROM_RESTORE_TABS_UI ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the given launch type explicitly specifies that the tab should belong to a
     * tab group.
     *
     * @param type The launch type to inspect.
     * @return True if the tab should be launched into a tab group.
     */
    public static boolean shouldLaunchAsGroupedTab(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_TAB_GROUP_UI,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type requires calculating adjacency relative to an opener or
     * parent tab rather than appending to the end of the tab model.
     *
     * @param type The launch type to inspect.
     * @return True if the new tab should be placed adjacent to an opener or parent tab.
     */
    public static boolean shouldOpenAdjacent(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_LINK,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    TabLaunchType.FROM_LONGPRESS_INCOGNITO,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND,
                    TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND ->
                    true;
            default -> false;
        };
    }

    /**
     * Returns true if the launch type represents a tab being reparented into a new window or
     * activity (either in the foreground or background).
     *
     * @param type The launch type to inspect.
     * @return True if the tab is being reparented into a window.
     */
    public static boolean isReparentingLaunch(@TabLaunchType int type) {
        assertValidLaunchType(type);
        return switch (type) {
            case TabLaunchType.FROM_REPARENTING, TabLaunchType.FROM_REPARENTING_BACKGROUND -> true;
            default -> false;
        };
    }
}
