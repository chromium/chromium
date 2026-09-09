// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateAttributesRegistry;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

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
    @Mock private Callback<Tab> mOnTabSelected;
    @Mock private LayoutManager mLayoutManager;
    @Mock private Activity mActivity;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Before
    public void setUp() {
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier())
                .thenReturn(ObservableSuppliers.createMonotonic(mTabModel));
        when(mTabModelSelector.getModels()).thenReturn(Collections.singletonList(mTabModel));
    }

    @After
    public void tearDown() {
        ActorKeyedServiceFactory.setForTesting(null);
        GlicKeyedServiceFactory.setForTesting(null);
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

    @Test
    public void testSelectTabAndShow_nullSelector() {
        assertNull(ActorTabStateHelper.selectTabAndShow(null, mLayoutManager, TAB_ID));
    }

    @Test
    public void testSelectTabAndShow_tabNotFound() {
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null);
        assertNull(ActorTabStateHelper.selectTabAndShow(mTabModelSelector, mLayoutManager, TAB_ID));
    }

    @Test
    public void testSelectTabAndShow_tabFound() {
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModel.indexOf(mTab)).thenReturn(1);

        Tab selected =
                ActorTabStateHelper.selectTabAndShow(mTabModelSelector, mLayoutManager, TAB_ID);

        assertEquals(mTab, selected);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);
    }

    @Test
    public void testSelectTabAndShow_hubVisible_switchesToBrowsing() {
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModel.indexOf(mTab)).thenReturn(1);
        when(mLayoutManager.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        ActorTabStateHelper.selectTabAndShow(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mLayoutManager).showLayout(LayoutType.BROWSING, false);
    }

    @Test
    public void testListenAndSelectTabOnAdded_nullSelector() {
        ActorTabStateHelper.listenAndSelectTabOnAdded(null, mLayoutManager, TAB_ID);
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void
            testListenAndSelectTabOnAdded_alreadyInitialized_tabNotFound_attachesObserverAndCleansUp() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mTabModel).removeObserver(tabModelObserver);
        verify(mTabModelSelector, never()).selectModel(anyBoolean());
    }

    @Test
    public void testListenAndSelectTabOnAdded_alreadyInitialized_tabExists_selectsImmediately() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelSelector, never()).addObserver(any());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testListenAndSelectTabOnAdded_alreadyExists_selectsImmediately() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelSelector, never()).addObserver(any());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testListenAndSelectTabOnAdded_didAddTab_matchingId() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didAddTab(
                mTab,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.LIVE_IN_BACKGROUND,
                /* markedForSelection= */ false);

        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModel).removeObserver(observer);
    }

    @Test
    public void testListenAndSelectTabOnAdded_didAddTab_nonMatchingId_doesNotSelect() {
        Tab otherTab = mock(Tab.class);
        when(otherTab.getId()).thenReturn(999);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didAddTab(
                otherTab,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.LIVE_IN_BACKGROUND,
                /* markedForSelection= */ false);

        verify(mTabModelSelector, never()).selectModel(anyBoolean());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
        verify(mTabModel, never()).removeObserver(any());
    }

    @Test
    public void testListenAndSelectTabOnAdded_tabStateInitialized_destroysObserver() {
        ArgumentCaptor<TabModelSelectorObserver> selectorObserverCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        verify(mTabModelSelector).addObserver(selectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = selectorObserverCaptor.getValue();

        selectorObserver.onTabStateInitialized();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mTabModel).removeObserver(tabModelObserver);
        verify(mTabModelSelector, never()).selectModel(anyBoolean());
    }

    @Test
    public void testListenAndSelectTabOnAdded_invalidTabId_returnsNullImmediately() {
        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, Tab.INVALID_TAB_ID, mOnTabSelected);

        verify(mOnTabSelected).onResult(null);
        verify(mTabModelSelector, never()).getModel(anyBoolean());
        verify(mTabModelSelector, never()).addObserver(any());
    }

    @Test
    public void testListenAndSelectTabOnAdded_withCallback_alreadyExists_invokesCallback() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mOnTabSelected).onResult(mTab);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    public void testListenAndSelectTabOnAdded_withCallback_didAddTab_invokesCallback() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ArgumentCaptor<TabModelSelectorObserver> selectorObserverCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        verify(mTabModelSelector).addObserver(selectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = selectorObserverCaptor.getValue();

        observer.didAddTab(
                mTab,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.LIVE_IN_BACKGROUND,
                /* markedForSelection= */ false);

        selectorObserver.onTabStateInitialized();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mOnTabSelected, times(1)).onResult(mTab);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModel).removeObserver(observer);
    }

    @Test
    public void
            testListenAndSelectTabOnAdded_withCallback_restoredOnTabStateInitialized_invokesCallback() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(0);

        ArgumentCaptor<TabModelSelectorObserver> selectorObserverCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mTabModelSelector).addObserver(selectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = selectorObserverCaptor.getValue();

        selectorObserver.onTabStateInitialized();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mOnTabSelected, times(1)).onResult(mTab);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    public void testListenAndSelectTabOnAdded_withCallback_nullSelector_invokesWithNull() {
        ActorTabStateHelper.listenAndSelectTabOnAdded(null, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mOnTabSelected).onResult(null);
    }

    @Test
    public void testListenAndSelectTabOnAdded_withCallback_notFound_invokesWithNull() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getTabById(TAB_ID)).thenReturn(null);

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mOnTabSelected).onResult(null);
    }

    @Test
    public void testPersistTabsForCompletedTask() {
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(101);
        when(tab1.isDestroyed()).thenReturn(false);
        when(tab1.getUrl()).thenReturn(GURL.emptyGURL());
        UserDataHost host1 = new UserDataHost();
        when(tab1.getUserDataHost()).thenReturn(host1);
        TabStateAttributesRegistry.createAttributesForTab(
                tab1, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_BACKGROUND);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(102);
        when(tab2.isDestroyed()).thenReturn(false);
        when(tab2.getUrl()).thenReturn(GURL.emptyGURL());
        UserDataHost host2 = new UserDataHost();
        when(tab2.getUserDataHost()).thenReturn(host2);
        TabStateAttributesRegistry.createAttributesForTab(
                tab2, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_BACKGROUND);

        BackgroundSession session = new BackgroundSession(tab1, 500);
        session.addTab(tab2);

        List<BackgroundSession> sessions = Collections.singletonList(session);

        // Before completion, attributes are UNTIDY
        TabStateAttributes attr1 =
                TabStateAttributesRegistry.getAttributesFor(
                        tab1, TabStateAttributes.StoreKey.class);
        TabStateAttributes attr2 =
                TabStateAttributesRegistry.getAttributesFor(
                        tab2, TabStateAttributes.StoreKey.class);
        assertEquals(DirtinessState.UNTIDY, attr1.getDirtinessState());
        assertEquals(DirtinessState.UNTIDY, attr2.getDirtinessState());

        // Calling persistTabsForCompletedTask transitions both to DIRTY
        ActorTabStateHelper.persistTabsForCompletedTask(sessions, 500);

        assertEquals(DirtinessState.DIRTY, attr1.getDirtinessState());
        assertEquals(DirtinessState.DIRTY, attr2.getDirtinessState());

        // Calling with unknown taskId is a no-op
        ActorTabStateHelper.persistTabsForCompletedTask(sessions, 999);
    }

    @Test
    public void testMaybeInvokeGlic_validConversationId_invokesGlic() {
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);

        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(
                mActivity, mTabModelSelector, supplier, mTab, "test_glic_conv_id");

        verify(mGlicKeyedService)
                .invokeWithConversation(
                        mTab,
                        "test_glic_conv_id",
                        GlicKeyedService.GlicInvocationSource.TOOLBAR_BUTTON);
    }

    @Test
    public void testMaybeInvokeGlic_emptyGlicConversationId_noOp() {
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(mActivity, mTabModelSelector, supplier, mTab, null);
        GlicKeyedService.maybeInvokeGlic(mActivity, mTabModelSelector, supplier, mTab, "");

        verify(mGlicKeyedService, never()).invokeWithConversation(any(), any(), anyInt());
    }

    @Test
    public void testMaybeInvokeGlic_incognitoTab_switchesModel() {
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mTab.isIncognito()).thenReturn(true);
        Tab regularTab = mock(Tab.class);
        when(regularTab.isIncognito()).thenReturn(false);
        when(regularTab.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.getCurrentTab()).thenReturn(regularTab);
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);

        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(
                mActivity, mTabModelSelector, supplier, mTab, "test_glic_conv_id");

        verify(mTabModelSelector).selectModel(false);
        verify(mGlicKeyedService)
                .invokeWithConversation(
                        regularTab,
                        "test_glic_conv_id",
                        GlicKeyedService.GlicInvocationSource.TOOLBAR_BUTTON);
    }

    @Test
    public void testMaybeInvokeGlic_activityFinishing_noOp() {
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mActivity.isFinishing()).thenReturn(true);

        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(
                mActivity, mTabModelSelector, supplier, mTab, "test_glic_conv_id");

        verify(mGlicKeyedService, never()).invokeWithConversation(any(), any(), anyInt());
    }

    @Test
    public void testMaybeInvokeGlic_tabDestroyed_noOp() {
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(true);
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);

        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(
                mActivity, mTabModelSelector, supplier, mTab, "test_glic_conv_id");

        verify(mGlicKeyedService, never()).invokeWithConversation(any(), any(), anyInt());
    }
}
