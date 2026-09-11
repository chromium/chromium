// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;

/** Unit tests for {@link LiveBackgroundTab}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LiveBackgroundTabTest {
    private static final int TAB_ID = 101;
    private static final int PLACEHOLDER_TAB_ID = 202;
    private static final int TASK_ID = 42;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BackgroundTabPool mPool;
    @Mock private Tab mTab;
    @Mock private Tab mPlaceholderTab;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private WebContentsState mPlaceholderContentsState;

    private LiveBackgroundTab mLiveBackgroundTab;

    @Before
    public void setUp() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.hasParentCollection()).thenReturn(false);

        when(mPlaceholderTab.getId()).thenReturn(PLACEHOLDER_TAB_ID);
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
    }

    @After
    public void tearDown() {
        OffscreenRenderingManager.getInstance().destroy();
    }

    @Test
    public void testGetters() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID, /* originalTabIndex= */ 3);
        assertSame(mTab, mLiveBackgroundTab.getTab());
        assertEquals(PLACEHOLDER_TAB_ID, mLiveBackgroundTab.getPlaceholderTabId());
        assertEquals(Integer.valueOf(TASK_ID), mLiveBackgroundTab.getTaskId());
        assertEquals(3, mLiveBackgroundTab.getOriginalTabIndex());
    }

    @Test
    public void testAttachTab_attachesAndRemovesFromPool() {
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        mLiveBackgroundTab = new LiveBackgroundTab(mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID);

        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        Tab attached = mLiveBackgroundTab.attachTab(mTabModel, 2);

        assertSame(mTab, attached);
        verify(mTab).removeObserver(observer);
        verify(mTabModel)
                .addTab(mTab, 2, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_BACKGROUND);
        verify(mPool).removeTabById(TAB_ID);

        assertThrows(AssertionError.class, () -> mLiveBackgroundTab.attachTab(mTabModel, 2));
    }

    @Test
    public void testAttachTab_withPlaceholderState_destroysContentsStateAndTransfersMetadata() {
        mLiveBackgroundTab = new LiveBackgroundTab(mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID);

        Token placeholderGroupToken = new Token(3L, 4L);
        TabState placeholderState = new TabState();
        placeholderState.contentsState = mPlaceholderContentsState;
        placeholderState.tabGroupId = placeholderGroupToken;
        placeholderState.rootId = 555;
        placeholderState.isPinned = true;

        Tab attached = mLiveBackgroundTab.attachTab(mTabModel, 2, placeholderState);

        assertSame(mTab, attached);
        verify(mTab).setTabGroupId(placeholderGroupToken);
        verify(mTab).setRootId(555);
        verify(mPlaceholderContentsState).destroy();
        assertNull(placeholderState.contentsState);
        verify(mTabModel).pinTab(TAB_ID, /* showUngroupDialog= */ false);
        verify(mTabModel).moveTab(TAB_ID, 2);
        verify(mTabModel)
                .addTab(mTab, 2, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_BACKGROUND);
        verify(mPool).removeTabById(TAB_ID);
    }

    @Test
    public void testAttachTab_withPlaceholderInModel_removesPlaceholder() {
        mLiveBackgroundTab = new LiveBackgroundTab(mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID);

        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);

        Tab attached = mLiveBackgroundTab.attachTab(mTabModel, 2, /* placeholderTabState= */ null);

        assertSame(mTab, attached);
        verify(mTabRemover).removeTab(mPlaceholderTab, /* allowDialog= */ false);
        verify(mPlaceholderTab).destroy();
        verify(mPool).removeTabById(TAB_ID);
    }

    @Test
    public void testAttachToForeground_withPlaceholder_replacesPlaceholderAndTransfersState() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID, /* originalTabIndex= */ 1);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(1);
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(mPlaceholderTab);

        Tab attached =
                mLiveBackgroundTab.attachToForeground(
                        mTabModel, mWindowAndroid, mTabDelegateFactory);

        assertSame(mTab, attached);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabModel)
                .addTab(mTab, 1, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
        verify(mPool).removeTabById(TAB_ID);
    }

    @Test
    public void testAttachToForeground_withoutPlaceholder_appendsAtOriginalIndex() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool, mTab, Tab.INVALID_TAB_ID, TASK_ID, /* originalTabIndex= */ 2);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(5);

        Tab attached =
                mLiveBackgroundTab.attachToForeground(
                        mTabModel, mWindowAndroid, mTabDelegateFactory);

        assertSame(mTab, attached);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabModel)
                .addTab(mTab, 2, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mTabRemover, never()).removeTab(any(), anyBoolean());
        verify(mPool).removeTabById(TAB_ID);
    }

    @Test
    public void testAttachToForeground_withoutPlaceholderAndInvalidOriginalIndex_appendsAtEnd() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool,
                        mTab,
                        Tab.INVALID_TAB_ID,
                        TASK_ID,
                        /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(4);

        Tab attached =
                mLiveBackgroundTab.attachToForeground(
                        mTabModel, mWindowAndroid, mTabDelegateFactory);

        assertSame(mTab, attached);
        verify(mTabModel)
                .addTab(mTab, 4, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mPool).removeTabById(TAB_ID);
    }

    @Test
    public void testTabDestruction_evictsFromPool() {
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        mLiveBackgroundTab = new LiveBackgroundTab(mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID);

        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onDestroyed(mTab);
        verify(mPool).removeLiveTabByOriginalId(TAB_ID);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testAttachToForeground_unregistersDestructionObserver() {
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        mLiveBackgroundTab = new LiveBackgroundTab(mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID);

        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        mLiveBackgroundTab.attachToForeground(mTabModel, mWindowAndroid, mTabDelegateFactory);

        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testAttachToForeground_withPlaceholder_pinnedTab_transfersPinnedState() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID, /* originalTabIndex= */ 1);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(1);
        when(mPlaceholderTab.getIsPinned()).thenReturn(true);

        mLiveBackgroundTab.attachToForeground(mTabModel, mWindowAndroid, mTabDelegateFactory);

        verify(mTabModel).pinTab(TAB_ID, false);
        verify(mTabModel).moveTab(TAB_ID, 2);
    }

    @Test
    public void testAttachToForeground_withPlaceholder_tabGroup_transfersTabGroup() {
        mLiveBackgroundTab =
                new LiveBackgroundTab(
                        mPool, mTab, PLACEHOLDER_TAB_ID, TASK_ID, /* originalTabIndex= */ 1);

        Token groupId = Token.createRandom();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(1);
        when(mPlaceholderTab.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getRelatedTabList(PLACEHOLDER_TAB_ID))
                .thenReturn(Collections.singletonList(mPlaceholderTab));

        mLiveBackgroundTab.attachToForeground(mTabModel, mWindowAndroid, mTabDelegateFactory);

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(mTab)),
                        eq(mPlaceholderTab),
                        eq(1),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));
    }
}
