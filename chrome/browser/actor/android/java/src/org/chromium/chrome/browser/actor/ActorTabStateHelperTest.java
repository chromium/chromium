// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ActorTabStateHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActorTabStateHelperTest {
    private static final int TAB_ID = 100;
    private static final boolean IS_PINNED = false;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private TabCreator mTabCreator;
    @Mock private Tab mPlaceholderTab;
    @Mock private Callback<Tab> mOnTabDetaching;

    @Before
    public void setUp() {
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
    }

    @After
    public void tearDown() {
        ActorKeyedServiceFactory.setForTesting(null);
        TabStateExtractor.resetTabStatesForTesting();
    }

    private void setupModelSelectorAndProfile() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
    }

    private void setupPlaceholderCreationMocks(boolean isPinned) {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getIsPinned()).thenReturn(isPinned);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabCreator.createFrozenTab(any(), anyInt(), eq(1))).thenReturn(mPlaceholderTab);
    }

    private void setupDetachmentMocks() {
        setupModelSelectorAndProfile();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());
        when(mActorKeyedService.getActiveTaskIdOnTab(TAB_ID, false)).thenReturn(500);
        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        setupPlaceholderCreationMocks(IS_PINNED);
    }

    @Test
    public void testDetachActiveBackgroundSessions_WithActiveTask_TransitionsTab() {
        setupDetachmentMocks();

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        mTabModelSelector, 42, mOnTabDetaching);

        assertEquals(1, sessions.size());
        assertEquals(mTab, sessions.get(0).getLastActiveTab());
        assertEquals(Integer.valueOf(500), sessions.get(0).getTaskId());
        assertEquals(1, sessions.get(0).getTabDataList().size());
        assertEquals(
                Integer.valueOf(101),
                sessions.get(0).getTabDataList().get(0).getPlaceholderTabId());
        assertEquals(0, sessions.get(0).getTabDataList().get(0).getOriginalTabIndex());
        assertEquals(42, sessions.get(0).getTabDataList().get(0).getTabWindowId());

        InOrder inOrder = inOrder(mOnTabDetaching, mTabRemover);
        inOrder.verify(mOnTabDetaching).onResult(mTab);
        inOrder.verify(mTabRemover).removeTab(mTab, false);

        verify(mTabCreator).createFrozenTab(eq(testTabState), anyInt(), eq(1));
        verify(mTabModel, never()).pinTab(anyInt(), anyBoolean());
    }

    @Test
    public void testDetachActiveBackgroundSessions_NoActiveTask_NoTransition() {
        setupModelSelectorAndProfile();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(0);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        mTabModelSelector, 0, mOnTabDetaching);

        assertTrue(sessions.isEmpty());
        verify(mOnTabDetaching, never()).onResult(any());
        verify(mTabRemover, never()).removeTab(any(), eq(false));
    }

    @Test
    public void testDetachActiveBackgroundSessions_MultipleTabsSameTask_GroupedInSession() {
        setupDetachmentMocks();

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(102);
        when(tab2.getIsPinned()).thenReturn(false);

        when(mTabModel.iterator()).thenReturn(Arrays.asList(mTab, tab2).iterator());
        when(mActorKeyedService.getActiveTaskIdOnTab(102, false)).thenReturn(500);
        when(mTabModel.indexOf(tab2)).thenReturn(1);
        when(mTabCreator.createFrozenTab(any(), anyInt(), eq(2))).thenReturn(mPlaceholderTab);

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);
        TabStateExtractor.setTabStateForTesting(102, testTabState);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        mTabModelSelector, 0, mOnTabDetaching);

        assertEquals(1, sessions.size());
        assertEquals(2, sessions.get(0).getTabs().size());
        assertEquals(mTab, sessions.get(0).getTabs().get(0));
        assertEquals(tab2, sessions.get(0).getTabs().get(1));
        assertEquals(tab2, sessions.get(0).getLastActiveTab());
        assertEquals(0, sessions.get(0).getTabDataList().get(0).getOriginalTabIndex());
        assertEquals(1, sessions.get(0).getTabDataList().get(1).getOriginalTabIndex());

        InOrder inOrder = inOrder(mOnTabDetaching, mTabRemover);
        inOrder.verify(mOnTabDetaching).onResult(mTab);
        inOrder.verify(mTabRemover).removeTab(mTab, false);
        inOrder.verify(mOnTabDetaching).onResult(tab2);
        inOrder.verify(mTabRemover).removeTab(tab2, false);
    }

    @Test
    public void testCreateAndInsertPlaceholder_CreatesDormantPlaceholder() {
        setupPlaceholderCreationMocks(false);

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);

        ActorTabStateHelper.createAndInsertPlaceholder(mTab, mTabModel);

        verify(mTabCreator).createFrozenTab(eq(testTabState), anyInt(), eq(1));
        verify(mTabModel, never()).pinTab(anyInt(), anyBoolean());
    }

    @Test
    public void testCreateAndInsertPlaceholder_PinnedTab() {
        setupPlaceholderCreationMocks(true);
        when(mPlaceholderTab.getId()).thenReturn(101);

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);

        ActorTabStateHelper.createAndInsertPlaceholder(mTab, mTabModel);

        verify(mTabCreator).createFrozenTab(eq(testTabState), anyInt(), eq(1));
        verify(mTabModel).pinTab(eq(101), eq(false));
        verify(mTabModel).moveTab(eq(101), eq(1));
    }

    @Test
    public void testCreateAndInsertPlaceholder_TabGroup() {
        setupPlaceholderCreationMocks(false);
        Token groupId = Token.createRandom();
        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getRelatedTabList(TAB_ID)).thenReturn(Collections.singletonList(mTab));

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);

        ActorTabStateHelper.createAndInsertPlaceholder(mTab, mTabModel);

        verify(mTabCreator).createFrozenTab(eq(testTabState), anyInt(), eq(1));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(mPlaceholderTab)),
                        eq(mTab),
                        eq(1),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));
    }

    @Test
    public void testBackgroundSession_addTabData_storesOriginalIndex() {
        BackgroundSession session = new BackgroundSession(mTab, 500);
        assertEquals(
                TabModel.INVALID_TAB_INDEX, session.getTabDataList().get(0).getOriginalTabIndex());

        Tab otherTab = mock(Tab.class);
        session.addTabData(new BackgroundSession.BackgroundTabData(otherTab, 102, 3, 42));

        assertEquals(2, session.getTabDataList().size());
        assertEquals(otherTab, session.getTabDataList().get(1).getTab());
        assertEquals(Integer.valueOf(102), session.getTabDataList().get(1).getPlaceholderTabId());
        assertEquals(3, session.getTabDataList().get(1).getOriginalTabIndex());
        assertEquals(42, session.getTabDataList().get(1).getTabWindowId());
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_RestoresMatchingTabs() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);

        WindowAndroid window = mock(WindowAndroid.class);
        TabDelegateFactory delegateFactory = mock(TabDelegateFactory.class);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);

        // Prepare background session with two tab metadatas:
        BackgroundSession session = new BackgroundSession(tab1, 500);
        session.addTab(tab2);

        // Metadata 1 (Window 1, Placeholder 101, Index 0)
        BackgroundSession.BackgroundTabData meta1 = session.getTabDataList().get(0);
        meta1.setTabWindowId(1);
        meta1.setPlaceholderTabId(101);
        meta1.setOriginalTabIndex(0);

        // Metadata 2 (Window 2, Placeholder 201, Index 1)
        BackgroundSession.BackgroundTabData meta2 = session.getTabDataList().get(1);
        meta2.setTabWindowId(2);
        meta2.setPlaceholderTabId(201);
        meta2.setOriginalTabIndex(1);

        // Set up the TabModel mock to contain the placeholder for tab1, but not tab2 (since it's a
        // different window)
        Tab placeholder1 = mock(Tab.class);
        when(mTabModel.getTabById(101)).thenReturn(placeholder1);
        when(mTabModel.indexOf(placeholder1)).thenReturn(3);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);

        // Tab 1 is currently not in mTabModel
        when(mTabModel.indexOf(tab1)).thenReturn(TabModel.INVALID_TAB_INDEX);

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));

        // Restore tabs for Window 1
        ActorTabStateHelper.restoreActiveWindowBackgroundTabs(
                mTabModelSelector, 1, window, sessions, delegateFactory);

        // Assertions:
        // Tab 1 (Window 1) should be restored
        verify(tab1).updateAttachment(window, delegateFactory);
        verify(mTabRemover).removeTab(placeholder1, false);
        verify(mTabModel)
                .addTab(
                        eq(tab1),
                        eq(3),
                        eq(TabLaunchType.FROM_RESTORE),
                        eq(TabCreationState.LIVE_IN_FOREGROUND));

        // Tab 2 (Window 2) should NOT be restored
        verify(tab2, never()).updateAttachment(any(), any());

        // Session should still exist because Tab 2 is not restored yet
        assertEquals(1, sessions.size());
        assertEquals(1, sessions.get(0).getTabDataList().size());
        assertEquals(tab2, sessions.get(0).getTabDataList().get(0).getTab());
    }
}
