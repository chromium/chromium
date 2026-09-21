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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;
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
    @Mock private ActorTask mActorTask;
    @Mock private ActorTask mMockTask;
    @Mock private Tab mTab2;
    @Mock private Tab mOtherTab;
    @Mock private Tab mTab1;
    @Mock private Tab mRegularTab;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private Tab mExistingTabInModel;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;

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

        when(mActorTask.getId()).thenReturn(500);
        when(mActorTask.isUnderActorControl()).thenReturn(true);
        when(mActorTask.getTabs()).thenReturn(Collections.singleton(TAB_ID));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);

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
    public void testDetachActiveBackgroundSessions_TasksNotRunning_NoTransition() {
        setupModelSelectorAndProfile();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);

        when(mMockTask.getId()).thenReturn(500);
        when(mMockTask.isUnderActorControl()).thenReturn(false);
        when(mMockTask.getTabs()).thenReturn(Collections.singleton(TAB_ID));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mMockTask));

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

        when(mTab2.getId()).thenReturn(102);
        when(mTab2.getIsPinned()).thenReturn(false);

        when(mTabModel.iterator()).thenReturn(Arrays.asList(mTab, mTab2).iterator());
        when(mTabModel.getTabById(102)).thenReturn(mTab2);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabCreator.createFrozenTab(any(), anyInt(), eq(2))).thenReturn(mPlaceholderTab);

        when(mMockTask.getId()).thenReturn(500);
        when(mMockTask.isUnderActorControl()).thenReturn(true);
        when(mMockTask.getTabs()).thenReturn(new LinkedHashSet<>(Arrays.asList(TAB_ID, 102)));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mMockTask));

        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID, testTabState);
        TabStateExtractor.setTabStateForTesting(102, testTabState);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        mTabModelSelector, 0, mOnTabDetaching);

        assertEquals(1, sessions.size());
        assertEquals(2, sessions.get(0).getTabs().size());
        assertEquals(mTab, sessions.get(0).getTabs().get(0));
        assertEquals(mTab2, sessions.get(0).getTabs().get(1));
        assertEquals(mTab2, sessions.get(0).getLastActiveTab());
        assertEquals(0, sessions.get(0).getTabDataList().get(0).getOriginalTabIndex());
        assertEquals(1, sessions.get(0).getTabDataList().get(1).getOriginalTabIndex());

        InOrder inOrder = inOrder(mOnTabDetaching, mTabRemover);
        inOrder.verify(mOnTabDetaching).onResult(mTab);
        inOrder.verify(mTabRemover).removeTab(mTab, false);
        inOrder.verify(mOnTabDetaching).onResult(mTab2);
        inOrder.verify(mTabRemover).removeTab(mTab2, false);
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

        session.addTabData(new BackgroundSession.BackgroundTabData(mOtherTab, 102, 3, 42));

        assertEquals(2, session.getTabDataList().size());
        assertEquals(mOtherTab, session.getTabDataList().get(1).getTab());
        assertEquals(Integer.valueOf(102), session.getTabDataList().get(1).getPlaceholderTabId());
        assertEquals(3, session.getTabDataList().get(1).getOriginalTabIndex());
        assertEquals(42, session.getTabDataList().get(1).getTabWindowId());
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
        when(mOtherTab.getId()).thenReturn(999);

        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didAddTab(
                mOtherTab,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.LIVE_IN_BACKGROUND,
                /* markedForSelection= */ false);

        verify(mTabModelSelector, never()).selectModel(anyBoolean());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
        verify(mTabModel, never()).removeObserver(any());
    }

    @Test
    public void testListenAndSelectTabOnAdded_tabStateInitialized_destroysObserver() {
        ActorTabStateHelper.listenAndSelectTabOnAdded(mTabModelSelector, mLayoutManager, TAB_ID);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();

        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = mSelectorObserverCaptor.getValue();

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

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = mSelectorObserverCaptor.getValue();

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

        ActorTabStateHelper.listenAndSelectTabOnAdded(
                mTabModelSelector, mLayoutManager, TAB_ID, mOnTabSelected);

        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        TabModelSelectorObserver selectorObserver = mSelectorObserverCaptor.getValue();

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
        when(mTab1.getId()).thenReturn(101);
        when(mTab1.isDestroyed()).thenReturn(false);
        when(mTab1.getUrl()).thenReturn(GURL.emptyGURL());
        UserDataHost host1 = new UserDataHost();
        when(mTab1.getUserDataHost()).thenReturn(host1);
        TabStateAttributesRegistry.createAttributesForTab(
                mTab1, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_BACKGROUND);

        when(mTab2.getId()).thenReturn(102);
        when(mTab2.isDestroyed()).thenReturn(false);
        when(mTab2.getUrl()).thenReturn(GURL.emptyGURL());
        UserDataHost host2 = new UserDataHost();
        when(mTab2.getUserDataHost()).thenReturn(host2);
        TabStateAttributesRegistry.createAttributesForTab(
                mTab2, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_BACKGROUND);

        BackgroundSession session = new BackgroundSession(mTab1, 500);
        session.addTab(mTab2);

        List<BackgroundSession> sessions = Collections.singletonList(session);

        // Before completion, attributes are UNTIDY
        TabStateAttributes attr1 =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab1, TabStateAttributes.StoreKey.class);
        TabStateAttributes attr2 =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab2, TabStateAttributes.StoreKey.class);
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
        when(mRegularTab.isIncognito()).thenReturn(false);
        when(mRegularTab.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mRegularTab);
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);

        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        supplier.set(mProfileProvider);

        GlicKeyedService.maybeInvokeGlic(
                mActivity, mTabModelSelector, supplier, mTab, "test_glic_conv_id");

        verify(mTabModelSelector).selectModel(false);
        verify(mGlicKeyedService)
                .invokeWithConversation(
                        mRegularTab,
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

    @Test
    @EnableFeatures({
        ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false",
        ChromeFeatureList.GLIC_BACKGROUND_ACTUATION_TAB_GROUP_SYNC
    })
    public void testSetTabGroupSyncPaused() {
        ActorTabStateHelper.setTabGroupSyncPaused(mTabGroupSyncService, true);
        verify(mTabGroupSyncService).setLocalObservationMode(false);

        ActorTabStateHelper.setTabGroupSyncPaused(mTabGroupSyncService, false);
        verify(mTabGroupSyncService).setLocalObservationMode(true);

        // Null syncService should be safely handled as a no-op
        ActorTabStateHelper.setTabGroupSyncPaused(null, true);
        ActorTabStateHelper.setTabGroupSyncPaused(null, false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION_TAB_GROUP_SYNC)
    public void testTabGroupSync_flagDisabled_doesNotInteractWithSync() {
        assertNull(ActorTabStateHelper.getTabGroupSyncService(mTabModel));

        ActorTabStateHelper.setTabGroupSyncPaused(mTabGroupSyncService, true);
        verify(mTabGroupSyncService, never()).setLocalObservationMode(anyBoolean());

        ActorTabStateHelper.updateTabGroupSyncMapping(mTabGroupSyncService, mTabModel, mTab, 202);
        verify(mTabGroupSyncService, never()).updateLocalTabId(any(), any(), anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false",
        ChromeFeatureList.GLIC_BACKGROUND_ACTUATION_TAB_GROUP_SYNC
    })
    public void testUpdateTabGroupSyncMapping() {
        Token tabGroupId = Token.createRandom();
        when(mTab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTabModel.tabGroupExists(tabGroupId)).thenReturn(true);

        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = localTabGroupId;

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = TAB_ID;
        savedTab.syncId = "test_sync_id_123";
        savedGroup.savedTabs.add(savedTab);

        when(mTabGroupSyncService.getGroup(localTabGroupId)).thenReturn(savedGroup);

        ActorTabStateHelper.updateTabGroupSyncMapping(mTabGroupSyncService, mTabModel, mTab, 202);

        verify(mTabGroupSyncService).updateLocalTabId(localTabGroupId, "test_sync_id_123", 202);
    }

    @Test
    public void testRestoreSessionTabToForeground_tabAlreadyPresentInModel_skipsAddTab() {
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mExistingTabInModel.getId()).thenReturn(TAB_ID);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mExistingTabInModel);

        ActorTabStateHelper.restoreSessionTabToForeground(
                mTab, 999, 0, mTabModel, mWindowAndroid, mTabDelegateFactory);

        verify(mTabModel, never()).addTab(any(), anyInt(), anyInt(), anyInt());
        verify(mTabRemover, never()).removeTab(any(), anyBoolean());
    }
}
