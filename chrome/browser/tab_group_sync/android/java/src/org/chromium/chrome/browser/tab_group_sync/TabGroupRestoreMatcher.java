// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Matches the open tabs of a foreign session to synced tab groups by the synced group ID that each
 * session tab carries. The class has no service dependencies so the Restore Tabs promo can call it
 * with a snapshot of both inputs.
 *
 * <p>Rules:
 *
 * <ul>
 *   <li>Archived groups never match.
 *   <li>Pinned session tabs never join a group.
 *   <li>A group takes its session tabs from one window: the window of its first unpinned tagged
 *       tab. Tagged tabs in other windows stay ungrouped.
 *   <li>Session tabs pair with the group's synced tabs first by equal filtered URL (ignoring the
 *       fragment), then in order: the remaining session tabs, in session order, take the remaining
 *       tabs, in group order. A tab paired in order is accepted even though its URL differs, and
 *       takes the position of the synced tab it fills.
 *   <li>Session tabs left over once every synced tab is paired stay ungrouped, so the local group
 *       never holds more tabs than the synced group.
 *   <li>Synced tabs left without a session tab get no entry. The restore fills them from sync.
 * </ul>
 */
@NullMarked
public final class TabGroupRestoreMatcher {
    private static final int INVALID_INDEX = -1;

    /** A foreign session tab, in visual order. */
    public static final class SessionTabRef {
        public final @Nullable GURL url;
        public final int windowIndex;
        public final boolean pinned;
        public final @Nullable String savedTabGroupGuid;

        /**
         * Creates a reference to a foreign tab.
         *
         * @param url The virtual URL of the tab's current navigation.
         * @param windowIndex The index of the foreign window holding the tab.
         * @param pinned Whether the tab is pinned on the source device.
         * @param savedTabGroupGuid The sync ID of the synced group the tab belonged to, if any.
         */
        public SessionTabRef(
                @Nullable GURL url,
                int windowIndex,
                boolean pinned,
                @Nullable String savedTabGroupGuid) {
            this.url = url;
            this.windowIndex = windowIndex;
            this.pinned = pinned;
            this.savedTabGroupGuid = savedTabGroupGuid;
        }
    }

    /** A synced group matched to foreign tabs. */
    public static final class GroupMatch {
        public final String syncId;
        public final boolean alreadyOpen;
        public final boolean shared;

        /**
         * Indices into the foreign tab list, in synced-group order. The restore must form the local
         * group in this order.
         */
        public final List<Integer> tabIndices;

        /** The synced tab ID that each {@link #tabIndices} entry fills. */
        public final List<String> savedTabSyncIds;

        private GroupMatch(
                String syncId,
                boolean alreadyOpen,
                boolean shared,
                List<Integer> tabIndices,
                List<String> savedTabSyncIds) {
            this.syncId = syncId;
            this.alreadyOpen = alreadyOpen;
            this.shared = shared;
            this.tabIndices = Collections.unmodifiableList(tabIndices);
            this.savedTabSyncIds = Collections.unmodifiableList(savedTabSyncIds);
        }
    }

    private TabGroupRestoreMatcher() {}

    /**
     * Returns the synced groups that match foreign tabs, ordered by their leftmost foreign tab.
     *
     * @param tabs The foreign tabs in visual order.
     * @param groups The synced groups to consider.
     */
    public static List<GroupMatch> match(List<SessionTabRef> tabs, List<SavedTabGroup> groups) {
        List<GroupMatch> matches = new ArrayList<>();
        for (SavedTabGroup group : groups) {
            GroupMatch match = matchGroup(tabs, group);
            if (match != null) matches.add(match);
        }
        Collections.sort(matches, Comparator.comparingInt(m -> Collections.min(m.tabIndices)));
        return matches;
    }

    /**
     * Returns whether {@code group} still pairs the same foreign tabs with the same synced tabs as
     * {@code match}, and is still open or closed and shared or not as it was.
     *
     * @param match A match returned by {@link #match}.
     * @param tabs The foreign tabs passed to {@link #match}.
     * @param group The current state of the matched synced group.
     */
    public static boolean stillMatches(
            GroupMatch match, List<SessionTabRef> tabs, SavedTabGroup group) {
        GroupMatch current = matchGroup(tabs, group);
        if (current == null) return false;

        return current.syncId.equals(match.syncId)
                && current.alreadyOpen == match.alreadyOpen
                && current.shared == match.shared
                && current.tabIndices.equals(match.tabIndices)
                && current.savedTabSyncIds.equals(match.savedTabSyncIds);
    }

    private static @Nullable GroupMatch matchGroup(List<SessionTabRef> tabs, SavedTabGroup group) {
        String syncId = group.syncId;
        if (TextUtils.isEmpty(syncId) || group.archivalTimeMs != null) return null;

        List<Integer> sessionIndices = findTaggedTabs(tabs, syncId);
        if (sessionIndices.isEmpty()) return null;

        List<SavedTabGroupTab> savedTabs = new ArrayList<>();
        List<String> savedTabIds = new ArrayList<>();
        for (SavedTabGroupTab savedTab : group.savedTabs) {
            String savedTabId = savedTab.syncId;
            if (TextUtils.isEmpty(savedTabId)) continue;
            savedTabs.add(savedTab);
            savedTabIds.add(savedTabId);
        }
        if (savedTabs.isEmpty()) return null;

        int[] sessionIndexForSlot = pair(tabs, sessionIndices, savedTabs);
        List<Integer> tabIndices = new ArrayList<>();
        List<String> savedTabSyncIds = new ArrayList<>();
        for (int slot = 0; slot < sessionIndexForSlot.length; slot++) {
            if (sessionIndexForSlot[slot] == INVALID_INDEX) continue;
            tabIndices.add(sessionIndexForSlot[slot]);
            savedTabSyncIds.add(savedTabIds.get(slot));
        }
        if (tabIndices.isEmpty()) return null;

        return new GroupMatch(
                syncId,
                /* alreadyOpen= */ group.localId != null,
                /* shared= */ !TextUtils.isEmpty(group.collaborationId),
                tabIndices,
                savedTabSyncIds);
    }

    /** Returns the unpinned tabs tagged with {@code syncId} in the window of the first one. */
    private static List<Integer> findTaggedTabs(List<SessionTabRef> tabs, String syncId) {
        List<Integer> indices = new ArrayList<>();
        int windowIndex = INVALID_INDEX;
        for (int i = 0; i < tabs.size(); i++) {
            SessionTabRef tab = tabs.get(i);
            if (tab.pinned || !syncId.equals(tab.savedTabGroupGuid)) continue;
            if (indices.isEmpty()) windowIndex = tab.windowIndex;
            if (tab.windowIndex != windowIndex) continue;
            indices.add(i);
        }
        return indices;
    }

    /**
     * Returns, for each synced tab, the index of the foreign tab paired with it, or {@link
     * #INVALID_INDEX} if none.
     */
    private static int[] pair(
            List<SessionTabRef> tabs,
            List<Integer> sessionIndices,
            List<SavedTabGroupTab> savedTabs) {
        int[] sessionIndexForSlot = new int[savedTabs.size()];
        Arrays.fill(sessionIndexForSlot, INVALID_INDEX);
        boolean[] paired = new boolean[sessionIndices.size()];

        for (int i = 0; i < sessionIndices.size(); i++) {
            GURL sessionUrl = normalizeUrl(tabs.get(sessionIndices.get(i)).url);
            if (sessionUrl == null) continue;
            for (int slot = 0; slot < savedTabs.size(); slot++) {
                if (sessionIndexForSlot[slot] != INVALID_INDEX) continue;
                GURL savedUrl = normalizeUrl(savedTabs.get(slot).url);
                if (savedUrl == null || !sessionUrl.equalsIgnoringRef(savedUrl)) continue;
                sessionIndexForSlot[slot] = sessionIndices.get(i);
                paired[i] = true;
                break;
            }
        }

        int slot = 0;
        for (int i = 0; i < sessionIndices.size(); i++) {
            if (paired[i]) continue;
            while (slot < sessionIndexForSlot.length
                    && sessionIndexForSlot[slot] != INVALID_INDEX) {
                slot++;
            }
            if (slot == sessionIndexForSlot.length) break;
            sessionIndexForSlot[slot] = sessionIndices.get(i);
        }
        return sessionIndexForSlot;
    }

    /**
     * Returns the URL filtered the same way tab group sync saves it, or null if missing or invalid.
     */
    private static @Nullable GURL normalizeUrl(@Nullable GURL url) {
        if (url == null || !url.isValid()) return null;
        return TabGroupSyncUtils.getFilteredUrlAndTitle(url, "").first;
    }
}
