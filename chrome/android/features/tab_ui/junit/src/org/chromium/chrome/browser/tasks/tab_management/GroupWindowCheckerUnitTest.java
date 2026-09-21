// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/** Unit tests for {@link GroupWindowChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupWindowCheckerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabModel mTabModel;
    @Mock private TabList mComprehensiveModel;

    private GroupWindowChecker mChecker;

    @Before
    public void setUp() {
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);
        mChecker = new GroupWindowChecker(mTabGroupSyncService, mTabModel);
    }

    private SavedTabGroup createSavedTabGroup(Token tabGroupId, String syncId) {
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(tabGroupId);
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.localId = 1;
        savedGroup.savedTabs = List.of(tab);
        return savedGroup;
    }

    @Test
    public void testHasOtherGroups_onlyCurrentGroup() {
        Token currentGroupId = Token.createRandom();
        SavedTabGroup group = createSavedTabGroup(currentGroupId, "sync1");

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync1"});
        when(mTabGroupSyncService.getGroup("sync1")).thenReturn(group);

        Tab tab = Mockito.mock(Tab.class);
        when(tab.getTabGroupId()).thenReturn(currentGroupId);
        when(tab.isClosing()).thenReturn(false);
        when(mComprehensiveModel.iterator()).thenReturn(List.of(tab).iterator());

        assertFalse(mChecker.hasOtherGroups(currentGroupId));
    }

    @Test
    public void testHasOtherGroups_withOtherGroup() {
        Token currentGroupId = Token.createRandom();
        Token otherGroupId = Token.createRandom();
        SavedTabGroup group1 = createSavedTabGroup(currentGroupId, "sync1");
        SavedTabGroup group2 = createSavedTabGroup(otherGroupId, "sync2");

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync1", "sync2"});
        when(mTabGroupSyncService.getGroup("sync1")).thenReturn(group1);
        when(mTabGroupSyncService.getGroup("sync2")).thenReturn(group2);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(currentGroupId);
        when(tab1.isClosing()).thenReturn(false);

        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getTabGroupId()).thenReturn(otherGroupId);
        when(tab2.isClosing()).thenReturn(false);

        when(mComprehensiveModel.iterator()).thenReturn(List.of(tab1, tab2).iterator());

        assertTrue(mChecker.hasOtherGroups(currentGroupId));
        assertTrue(mChecker.hasOtherGroups(/* currentGroupId= */ null));
    }

    @Test
    public void testHasOtherGroups_emptyGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        assertFalse(mChecker.hasOtherGroups(/* currentGroupId= */ null));
    }
}
