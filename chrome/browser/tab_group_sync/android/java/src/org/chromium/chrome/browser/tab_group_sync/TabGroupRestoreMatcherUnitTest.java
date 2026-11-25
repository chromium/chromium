// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_group_sync.TabGroupRestoreMatcher.GroupMatch;
import org.chromium.chrome.browser.tab_group_sync.TabGroupRestoreMatcher.SessionTabRef;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupId;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link TabGroupRestoreMatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupRestoreMatcherUnitTest {
    private static final String A = "https://a.com/";
    private static final String B = "https://b.com/";
    private static final String C = "https://c.com/";
    private static final String X = "https://x.com/";

    @Test
    public void testMatchesTaggedTabsInOrder() {
        List<SessionTabRef> tabs = List.of(tab(X, null), tab(A, "g1"), tab(B, "g1"));
        GroupMatch match = match(tabs, group("g1", A, B)).get(0);
        assertEquals(List.of(1, 2), match.tabIndices);
        assertEquals(List.of("g1_0", "g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testOutputFollowsSyncedOrder() {
        List<SessionTabRef> tabs = List.of(tab(C, "g1"), tab(X, "g1"));
        GroupMatch match = match(tabs, group("g1", A, B, C)).get(0);
        assertEquals(List.of(1, 0), match.tabIndices);
        assertEquals(List.of("g1_0", "g1_2"), match.savedTabSyncIds);
    }

    @Test
    public void testUrlMatchingIgnoresFragment() {
        List<SessionTabRef> tabs = List.of(tab(A + "#frag", "g1"));
        GroupMatch match = match(tabs, group("g1", B, A)).get(0);
        assertEquals(List.of(0), match.tabIndices);
        assertEquals(List.of("g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testNonSavableUrlMatchesFilteredSyncUrl() {
        List<SessionTabRef> tabs = List.of(tab("chrome-native://newtab/", "g1"));
        GroupMatch match = match(tabs, group("g1", B, "chrome://newtab/")).get(0);
        assertEquals(List.of(0), match.tabIndices);
        assertEquals(List.of("g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testDuplicateUrlsPairInOrder() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"), tab(A, "g1"));
        GroupMatch match = match(tabs, group("g1", A, A)).get(0);
        assertEquals(List.of(0, 1), match.tabIndices);
        assertEquals(List.of("g1_0", "g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testMissingSessionUrlFallsToOrderedFill() {
        SessionTabRef noUrl = new SessionTabRef(null, 0, /* pinned= */ false, "g1");
        GroupMatch match = match(List.of(noUrl), group("g1", A, B)).get(0);
        assertEquals(List.of(0), match.tabIndices);
        assertEquals(List.of("g1_0"), match.savedTabSyncIds);
    }

    @Test
    public void testInvalidSessionUrlFallsToOrderedFill() {
        SessionTabRef emptyUrl = new SessionTabRef(GURL.emptyGURL(), 0, /* pinned= */ false, "g1");
        GroupMatch match = match(List.of(emptyUrl), group("g1", A, B)).get(0);
        assertEquals(List.of(0), match.tabIndices);
        assertEquals(List.of("g1_0"), match.savedTabSyncIds);
    }

    @Test
    public void testPinnedTaggedTabIsExcluded() {
        List<SessionTabRef> tabs = List.of(pinned(A, "g1", 0), tab(B, "g1"));
        GroupMatch match = match(tabs, group("g1", A, B)).get(0);
        assertEquals(List.of(1), match.tabIndices);
        assertEquals(List.of("g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testTaggedTabsInOtherWindowsAreIgnored() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1", 0), tab(B, "g1", 1), tab(C, "g1", 0));
        GroupMatch match = match(tabs, group("g1", A, B, C)).get(0);
        assertEquals(List.of(0, 2), match.tabIndices);
        assertEquals(List.of("g1_0", "g1_2"), match.savedTabSyncIds);
    }

    @Test
    public void testPinnedFirstTabDoesNotPickWindow() {
        List<SessionTabRef> tabs = List.of(pinned(A, "g1", 1), tab(B, "g1", 0), tab(C, "g1", 1));
        GroupMatch match = match(tabs, group("g1", A, B, C)).get(0);
        assertEquals(List.of(1), match.tabIndices);
    }

    @Test
    public void testInterleavedGroupsMatchSeparately() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"), tab(B, "g2"), tab(C, "g1"), tab(X, "g2"));
        List<GroupMatch> matches = match(tabs, group("g2", B, X), group("g1", A, C));
        assertEquals("g1", matches.get(0).syncId);
        assertEquals(List.of(0, 2), matches.get(0).tabIndices);
        assertEquals(List.of("g1_0", "g1_1"), matches.get(0).savedTabSyncIds);
        assertEquals("g2", matches.get(1).syncId);
        assertEquals(List.of(1, 3), matches.get(1).tabIndices);
        assertEquals(List.of("g2_0", "g2_1"), matches.get(1).savedTabSyncIds);
    }

    @Test
    public void testAlreadyOpenGroupIsFlagged() {
        SavedTabGroup group = group("g1", A);
        group.localId = new LocalTabGroupId(TabGroupId.fromRawToken(new Token(1, 2)));
        assertTrue(match(List.of(tab(A, "g1")), group).get(0).alreadyOpen);
    }

    @Test
    public void testClosedGroupIsNotFlaggedOpen() {
        assertFalse(match(List.of(tab(A, "g1")), group("g1", A)).get(0).alreadyOpen);
    }

    @Test
    public void testSharedGroupIsFlagged() {
        SavedTabGroup group = group("g1", A);
        group.collaborationId = "collaboration";
        assertTrue(match(List.of(tab(A, "g1")), group).get(0).shared);
    }

    @Test
    public void testPrivateGroupIsNotFlaggedShared() {
        assertFalse(match(List.of(tab(A, "g1")), group("g1", A)).get(0).shared);
    }

    @Test
    public void testArchivedGroupDoesNotMatch() {
        SavedTabGroup group = group("g1", A);
        group.archivalTimeMs = 1L;
        assertTrue(match(List.of(tab(A, "g1")), group).isEmpty());
    }

    @Test
    public void testGroupWithoutTaggedTabsDoesNotMatch() {
        List<SessionTabRef> tabs = List.of(tab(A, null), tab(B, "g2"));
        assertTrue(match(tabs, group("g1", A, B)).isEmpty());
    }

    @Test
    public void testGroupWithoutSyncedTabsDoesNotMatch() {
        assertTrue(match(List.of(tab(A, "g1")), group("g1")).isEmpty());
    }

    @Test
    public void testGroupWithoutSyncIdDoesNotMatch() {
        SavedTabGroup group = group("g1", A);
        group.syncId = null;
        assertTrue(match(List.of(tab(A, "g1")), group).isEmpty());
    }

    @Test
    public void testGroupWithEmptySyncIdDoesNotMatch() {
        SavedTabGroup group = group("g1", A);
        group.syncId = "";
        assertTrue(match(List.of(tab(A, "")), group).isEmpty());
    }

    @Test
    public void testSyncedTabWithoutSyncIdIsSkipped() {
        SavedTabGroup group = group("g1", A, B);
        group.savedTabs.get(0).syncId = null;
        GroupMatch match = match(List.of(tab(A, "g1")), group).get(0);
        assertEquals(List.of("g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testSyncedTabWithEmptySyncIdIsSkipped() {
        SavedTabGroup group = group("g1", A, B);
        group.savedTabs.get(0).syncId = "";
        GroupMatch match = match(List.of(tab(A, "g1")), group).get(0);
        assertEquals(List.of("g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testExtraTaggedTabsAreDropped() {
        List<SessionTabRef> tabs = List.of(tab(X, "g1"), tab(A, "g1"), tab(B, "g1"));
        GroupMatch match = match(tabs, group("g1", A, B)).get(0);
        assertEquals(List.of(1, 2), match.tabIndices);
        assertEquals(List.of("g1_0", "g1_1"), match.savedTabSyncIds);
    }

    @Test
    public void testStillMatchesUnchangedGroup() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"), tab(B, "g1"));
        SavedTabGroup group = group("g1", A, B);
        GroupMatch match = match(tabs, group).get(0);
        assertTrue(TabGroupRestoreMatcher.stillMatches(match, tabs, group));
    }

    @Test
    public void testStillMatchesFailsAfterSyncedTabChange() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"), tab(B, "g1"));
        GroupMatch match = match(tabs, group("g1", A, B)).get(0);
        SavedTabGroup changed = group("g1", A);
        assertFalse(TabGroupRestoreMatcher.stillMatches(match, tabs, changed));
    }

    @Test
    public void testStillMatchesFailsAfterGroupOpens() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"));
        SavedTabGroup group = group("g1", A);
        GroupMatch match = match(tabs, group).get(0);
        group.localId = new LocalTabGroupId(TabGroupId.fromRawToken(new Token(1, 2)));
        assertFalse(TabGroupRestoreMatcher.stillMatches(match, tabs, group));
    }

    @Test
    public void testStillMatchesFailsAfterGroupIsShared() {
        List<SessionTabRef> tabs = List.of(tab(A, "g1"));
        SavedTabGroup group = group("g1", A);
        GroupMatch match = match(tabs, group).get(0);
        group.collaborationId = "collaboration";
        assertFalse(TabGroupRestoreMatcher.stillMatches(match, tabs, group));
    }

    private static List<GroupMatch> match(List<SessionTabRef> tabs, SavedTabGroup... groups) {
        return TabGroupRestoreMatcher.match(tabs, List.of(groups));
    }

    private static SessionTabRef tab(String url, @Nullable String groupId) {
        return tab(url, groupId, /* windowIndex= */ 0);
    }

    private static SessionTabRef tab(String url, @Nullable String groupId, int windowIndex) {
        return new SessionTabRef(new GURL(url), windowIndex, /* pinned= */ false, groupId);
    }

    private static SessionTabRef pinned(String url, String groupId, int windowIndex) {
        return new SessionTabRef(new GURL(url), windowIndex, /* pinned= */ true, groupId);
    }

    private static SavedTabGroup group(String syncId, String... urls) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = syncId;
        for (int i = 0; i < urls.length; i++) {
            SavedTabGroupTab savedTab = new SavedTabGroupTab();
            savedTab.syncId = syncId + "_" + i;
            savedTab.url = new GURL(urls[i]);
            group.savedTabs.add(savedTab);
        }
        return group;
    }
}
