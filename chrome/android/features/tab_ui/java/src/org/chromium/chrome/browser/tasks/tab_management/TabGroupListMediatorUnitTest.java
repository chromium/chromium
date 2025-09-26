// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID2;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID3;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests for {@link TabGroupListMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabGroupListMediatorUnitTest {
    private static final Token LOCAL_GROUP_ID1 = new Token(1, 1);
    private static final Token LOCAL_GROUP_ID2 = new Token(2, 2);
    private static final int ROOT_ID1 = 1;
    private static final int ROOT_ID2 = 2;
    private static final String GROUP_NAME1 = "Shopping";
    private static final String GROUP_NAME2 = "Travel";
    private static final String GROUP_NAME3 = "Chamber of Secrets";
    private static final String MESSAGE_ID1 = "MESSAGE_ID1";
    private static final String MESSAGE_ID2 = "MESSAGE_ID2";
    private static final String MESSAGE_ID3 = "MESSAGE_ID3";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabList mComprehensiveModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private PaneManager mPaneManager;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private TabSwitcherPaneBase mTabSwitcherPaneBase;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private SyncService mSyncService;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private PersistentVersioningMessageMediator mPersistentVersioningMessageMediator;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserver;
    @Captor private ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncObserverCaptor;

    @Captor
    private ArgumentCaptor<Callback<@ActionConfirmationResult Integer>>
            mActionConfirmationResultCallbackCaptor;

    @Captor
    private ArgumentCaptor<Callback<MaybeBlockingResult>> mMaybeBlockingResultCallbackCaptor;

    @Captor
    private ArgumentCaptor<SyncService.SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    @Captor private ArgumentCaptor<Callback<Boolean>> mDeleteGroupResultCallbackCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mModalPropertyModelCaptor;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;

    private PropertyModel mPropertyModel;
    private ModelList mModelList;
    private Context mContext;
    private SharedGroupTestHelper mSharedGroupTestHelper;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;
    private TabGroupRemovedMessageMediator mTabGroupRemovedMessageMediator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mPropertyModel = new PropertyModel(TabGroupListProperties.ALL_KEYS);
        mModelList = new ModelList();
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPaneBase);
        when(mTabSwitcherPaneBase.requestOpenTabGroupDialog(anyInt())).thenReturn(true);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        mSharedGroupTestHelper = new SharedGroupTestHelper(mCollaborationService);
        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);

        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);

        mTabGroupRemovedMessageMediator =
                new TabGroupRemovedMessageMediator(mContext, mMessagingBackendService, mModelList);
    }

    private TabGroupListMediator createMediator() {
        TabGroupListMediator mediator =
                new TabGroupListMediator(
                        mContext,
                        mModelList,
                        mPropertyModel,
                        mTabGroupModelFilter,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService,
                        mMessagingBackendService,
                        mPaneManager,
                        mTabGroupUiActionHandler,
                        mActionConfirmationManager,
                        mSyncService,
                        /* enableContainment= */ true,
                        mDataSharingTabManager,
                        mTabGroupRemovedMessageMediator,
                        mPersistentVersioningMessageMediator);
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        return mediator;
    }

    @Test
    public void testNoTabGroups() {
        createMediator();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testNoTabGroupSyncService() {
        mTabGroupSyncService = null;
        createMediator();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOneGroup() {
        SavedTabGroup group = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);

        createMediator();
        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals(
                new TabGroupRowViewTitleData(
                        "Title", 1, R.plurals.tab_group_row_accessibility_text),
                model.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, model.get(COLOR_INDEX));
    }

    @Test
    public void testTwoGroups() {
        SavedTabGroup fooGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        fooGroup.title = "Foo";
        fooGroup.color = TabGroupColorId.BLUE;
        fooGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(2);
        fooGroup.creationTimeMs = 1;
        fooGroup.savedTabs.get(0).updateTimeMs = 2;
        fooGroup.savedTabs.get(1).updateTimeMs = 1;

        SavedTabGroup barGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        barGroup.title = "Bar";
        barGroup.color = TabGroupColorId.RED;
        barGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(3);
        barGroup.creationTimeMs = 2;
        barGroup.savedTabs.get(0).updateTimeMs = 3;
        barGroup.savedTabs.get(1).updateTimeMs = 2;
        barGroup.savedTabs.get(2).updateTimeMs = 1;

        createMediator();
        assertEquals(2, mModelList.size());

        PropertyModel barModel = mModelList.get(0).model;
        assertEquals(
                new TabGroupRowViewTitleData("Bar", 3, R.plurals.tab_group_row_accessibility_text),
                barModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, barModel.get(COLOR_INDEX));

        // Ensure the tab groups are sorted by update time and NOT creation time.
        PropertyModel fooModel = mModelList.get(1).model;
        assertEquals(
                new TabGroupRowViewTitleData("Foo", 2, R.plurals.tab_group_row_accessibility_text),
                fooModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, fooModel.get(COLOR_INDEX));
    }

    @Test
    public void testSyncObservation() {
        SavedTabGroup group = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);

        createMediator();
        assertEquals(1, mModelList.size());

        mSyncedGroupTestHelper.removeTabGroup(SYNC_GROUP_ID1);
        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        mTabGroupSyncObserverCaptor
                .getValue()
                .onTabGroupRemoved(SYNC_GROUP_ID1, TriggerSource.LOCAL);
        ShadowLooper.idleMainLooper();

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testTabModelObservation() {
        SavedTabGroup group = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);

        createMediator();
        assertEquals(1, mModelList.size());

        mSyncedGroupTestHelper.removeTabGroup(SYNC_GROUP_ID1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        verify(mTabGroupModelFilter).addObserver(mTabModelObserver.capture());
        mTabModelObserver.getValue().tabClosureUndone(mTab1);
        ShadowLooper.idleMainLooper();

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testFilterOutOtherTabGroups() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.title = "in current";
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        group2.title = "in another";
        group2.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group2.localId = new LocalTabGroupId(LOCAL_GROUP_ID2);

        SavedTabGroup group3 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID3);
        group3.title = "hidden";
        group3.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group3.localId = null;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.tabGroupExists(LOCAL_GROUP_ID1)).thenReturn(true);
        when(mTabGroupModelFilter.tabGroupExists(LOCAL_GROUP_ID2)).thenReturn(false);
        List<Tab> tabList = List.of(mTab1);
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;
        assertEquals(
                new TabGroupRowViewTitleData(
                        "in current", 1, R.plurals.tab_group_row_accessibility_text),
                model1.get(TITLE_DATA));
        PropertyModel model2 = mModelList.get(1).model;
        assertEquals(
                new TabGroupRowViewTitleData(
                        "hidden", 1, R.plurals.tab_group_row_accessibility_text),
                model2.get(TITLE_DATA));
    }

    @Test
    public void testOpenRunnable() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        group2.syncId = SYNC_GROUP_ID2;
        group2.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group2.localId = null;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.tabGroupExists(LOCAL_GROUP_ID1)).thenReturn(true);
        when(mTabGroupModelFilter.tabGroupExists(LOCAL_GROUP_ID2)).thenReturn(false);
        List<Tab> tabList = List.of(mTab1);
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;
        model1.get(OPEN_RUNNABLE).run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID1);

        // Set up mocks to change behavior after #openTabGroup() is called.
        MockitoHelper.doRunnable(
                        () -> {
                            SavedTabGroup updatedGroup2 =
                                    mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
                            updatedGroup2.syncId = SYNC_GROUP_ID2;
                            updatedGroup2.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
                            updatedGroup2.localId = new LocalTabGroupId(LOCAL_GROUP_ID2);

                            when(mTabGroupModelFilter.tabGroupExists(LOCAL_GROUP_ID2))
                                    .thenReturn(true);
                            when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID2))
                                    .thenReturn(ROOT_ID2);
                        })
                .when(mTabGroupUiActionHandler)
                .openTabGroup(SYNC_GROUP_ID2);

        ShadowLooper.idleMainLooper();

        PropertyModel model2 = mModelList.get(1).model;
        model2.get(OPEN_RUNNABLE).run();
        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_GROUP_ID2);
        verify(mPaneManager, times(2)).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID2);
    }

    @Test
    public void testOpenRunnable_CurrentClosing_Racy() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromIds(ROOT_ID1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        List<Tab> tabList = List.of(mTab1);
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        // Pretend the closure was already committed so we need to fallback to the HIDDEN behavior.
        group1.localId = null;
        when(mComprehensiveModel.iterator())
                .thenAnswer(invocation -> Collections.emptyList().iterator());
        when(mComprehensiveModel.getCount()).thenReturn(0);
        MockitoHelper.doRunnable(
                        () -> {
                            group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
                            when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1))
                                    .thenReturn(ROOT_ID1);
                        })
                .when(mTabGroupUiActionHandler)
                .openTabGroup(SYNC_GROUP_ID1);

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_GROUP_ID1);
        verify(mTabModel, never()).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID1);
    }

    @Test
    public void testOpenRunnable_CurrentClosing_NoRace() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromIds(ROOT_ID1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        List<Tab> tabList = List.of(mTab1);
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabModel).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID1);
    }

    @Test
    public void testOpenRunnable_ClosingAfterShowing() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromIds(ROOT_ID1, ROOT_ID2);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        List<Tab> tabList = List.of(mTab1, mTab2);
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mComprehensiveModel.getCount()).thenReturn(2);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mComprehensiveModel.getTabAtChecked(1)).thenReturn(mTab2);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab2.getRootId()).thenReturn(ROOT_ID1);
        when(mTab2.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);

        createMediator();
        assertEquals(1, mModelList.size());

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabModel, never()).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID1);

        when(mTab1.isClosing()).thenReturn(true);
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabModel, never()).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager, times(2)).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase, times(2)).requestOpenTabGroupDialog(ROOT_ID1);

        when(mTab2.isClosing()).thenReturn(true);
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabModel).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager, times(3)).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase, times(3)).requestOpenTabGroupDialog(ROOT_ID1);
    }

    @Test
    public void testDeleteRunnable() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        group2.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group2.localId = null;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.getTabsInGroup(LOCAL_GROUP_ID1)).thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> List.of(mTab1).iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        // IN_CURRENT
        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;
        model1.get(DELETE_RUNNABLE).run();
        verify(mTabRemover).closeTabs(any(), anyBoolean());

        // HIDDEN - Negative
        PropertyModel model2 = mModelList.get(1).model;
        model2.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mActionConfirmationResultCallbackCaptor.capture());
        mActionConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mTabGroupSyncService, never()).removeGroup(SYNC_GROUP_ID2);

        // HIDDEN - Positive
        reset(mActionConfirmationManager);
        model2.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mActionConfirmationResultCallbackCaptor.capture());
        mActionConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID2);
    }

    @Test
    public void testDeleteRunnable_CurrentClosing() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromIds(ROOT_ID1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getTabsInGroup(LOCAL_GROUP_ID1)).thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> List.of(mTab1).iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        // IN_CURRENT_CLOSING
        PropertyModel model1 = mModelList.get(0).model;
        model1.get(DELETE_RUNNABLE).run();
        verify(mTabModel).commitTabClosure(ROOT_ID1);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID1);
    }

    @Test
    public void testEmptyStateEnabled() {
        createMediator();
        assertTrue(mPropertyModel.get(TabGroupListProperties.EMPTY_STATE_VISIBLE));

        SavedTabGroup group = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        mTabGroupSyncObserverCaptor
                .getValue()
                .onTabGroupRemoved(SYNC_GROUP_ID1, TriggerSource.LOCAL);
        ShadowLooper.idleMainLooper();
        assertFalse(mPropertyModel.get(TabGroupListProperties.EMPTY_STATE_VISIBLE));
    }

    @Test
    public void testSyncEnabled() {
        createMediator();
        assertFalse(mPropertyModel.get(TabGroupListProperties.SYNC_ENABLED));

        when(mSyncService.getActiveDataTypes())
                .thenReturn(Collections.singleton(DataType.SAVED_TAB_GROUP));
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();
        assertTrue(mPropertyModel.get(TabGroupListProperties.SYNC_ENABLED));
    }

    @Test
    public void testDestroy() {
        createMediator().destroy();

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupSyncService).removeObserver(any());
        verify(mSyncService).removeSyncStateChangedListener(any());

        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        reset(mTabGroupSyncService);
        mTabGroupSyncObserverCaptor.getValue().onTabGroupAdded(null, 0);
        ShadowLooper.idleMainLooper();
        verify(mTabGroupSyncService, never()).getAllGroupIds();

        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testDeleteRunnable_SharedGroup() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getTabsInGroup(LOCAL_GROUP_ID1)).thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> List.of(mTab1).iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);
        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;

        assertNotNull(model.get(DELETE_RUNNABLE));
        model.get(DELETE_RUNNABLE).run();

        verify(mDataSharingTabManager).leaveOrDeleteFlow(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testLeaveRunnable() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getTabsInGroup(LOCAL_GROUP_ID1)).thenReturn(List.of(mTab1));
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> List.of(mTab1).iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);

        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;

        assertNotNull(model.get(LEAVE_RUNNABLE));
        model.get(LEAVE_RUNNABLE).run();

        verify(mDataSharingTabManager).leaveOrDeleteFlow(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testDeleteRunnable_shareReadFailure() {
        SavedTabGroup group1 = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = SyncedGroupTestHelper.tabsFromCount(1);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupModelFilter.getGroupLastShownTabId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getTabsInGroup(LOCAL_GROUP_ID1)).thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.iterator()).thenAnswer(invocation -> List.of(mTab1).iterator());
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);
        mSharedGroupTestHelper.mockGetGroupDataFailure(COLLABORATION_ID1);

        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;

        assertNotNull(model.get(DELETE_RUNNABLE));
        model.get(DELETE_RUNNABLE).run();
        verify(mTabRemover).closeTabs(any(), eq(true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testNoTabGroupRemovedMessageCard() {
        List<PersistentMessage> messageList = List.of();
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);
        createMediator();
        assertEquals(0, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testNoGroupRemovedMessageCard_NullId() {
        PersistentMessage messageWithoutId = new PersistentMessage();
        messageWithoutId.attribution = new MessageAttribution();
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(List.of(messageWithoutId));

        createMediator();

        assertEquals(0, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCard() {
        List<PersistentMessage> messageList =
                List.of(makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        createMediator();

        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals("\"Shopping\" tab group no longer available", model.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model.get(CARD_TYPE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCardWithAtLeastOneUnnamedGroupTitle() {
        List<PersistentMessage> messageList =
                List.of(
                        makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1),
                        makeTabGroupRemovedMessage(MESSAGE_ID2, ""));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        createMediator();

        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals("2 tab groups no longer available", model.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model.get(CARD_TYPE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCardWithThreeGroupsRemoved() {
        List<PersistentMessage> messageList =
                List.of(
                        makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1),
                        makeTabGroupRemovedMessage(MESSAGE_ID2, GROUP_NAME2),
                        makeTabGroupRemovedMessage(MESSAGE_ID3, GROUP_NAME3));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        createMediator();

        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals("3 tab groups no longer available", model.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model.get(CARD_TYPE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCardWithOneUnnamedGroupTitle() {
        List<PersistentMessage> messageList = List.of(makeTabGroupRemovedMessage(MESSAGE_ID1, ""));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        createMediator();

        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals("1 tab group no longer available", model.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model.get(CARD_TYPE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCardsAndTabGroupsOrdering() {
        List<PersistentMessage> messageList =
                List.of(
                        makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1),
                        makeTabGroupRemovedMessage(MESSAGE_ID2, GROUP_NAME2));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);
        SavedTabGroup fooGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        fooGroup.title = "Foo";
        fooGroup.color = TabGroupColorId.BLUE;
        fooGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(2);
        fooGroup.creationTimeMs = 1;
        fooGroup.savedTabs.get(0).updateTimeMs = 1;
        fooGroup.savedTabs.get(1).updateTimeMs = 0;

        SavedTabGroup barGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        barGroup.title = "Bar";
        barGroup.color = TabGroupColorId.RED;
        barGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(3);
        barGroup.creationTimeMs = 2;
        barGroup.savedTabs.get(0).updateTimeMs = 3;
        barGroup.savedTabs.get(1).updateTimeMs = 2;
        barGroup.savedTabs.get(2).updateTimeMs = 1;

        createMediator();

        assertEquals(3, mModelList.size());

        PropertyModel model1 = mModelList.get(0).model;
        assertEquals(
                "\"Shopping\" and \"Travel\" tab groups no longer available",
                model1.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model1.get(CARD_TYPE));

        // Ensure the tab groups are sorted by update time and NOT creation time.
        PropertyModel barModel = mModelList.get(1).model;
        assertEquals(
                new TabGroupRowViewTitleData("Bar", 3, R.plurals.tab_group_row_accessibility_text),
                barModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, barModel.get(COLOR_INDEX));

        PropertyModel fooModel = mModelList.get(2).model;
        assertEquals(
                new TabGroupRowViewTitleData("Foo", 2, R.plurals.tab_group_row_accessibility_text),
                fooModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, fooModel.get(COLOR_INDEX));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DATA_SHARING,
        ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS
    })
    public void testTabGroupRemovedMessageCardsAndTabGroupsOrdering_ByRecency() {
        List<PersistentMessage> messageList =
                List.of(
                        makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1),
                        makeTabGroupRemovedMessage(MESSAGE_ID2, GROUP_NAME2));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        SavedTabGroup fooGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        fooGroup.title = "Foo";
        fooGroup.color = TabGroupColorId.BLUE;
        fooGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(2);
        fooGroup.creationTimeMs = 1;
        fooGroup.savedTabs.get(0).updateTimeMs = 3;
        fooGroup.savedTabs.get(1).updateTimeMs = 2;

        SavedTabGroup barGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        barGroup.title = "Bar";
        barGroup.color = TabGroupColorId.RED;
        barGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(3);
        barGroup.creationTimeMs = 2;
        barGroup.savedTabs.get(0).updateTimeMs = 2;
        barGroup.savedTabs.get(1).updateTimeMs = 1;
        barGroup.savedTabs.get(2).updateTimeMs = 0;

        createMediator();

        assertEquals(3, mModelList.size());

        PropertyModel model1 = mModelList.get(0).model;
        assertEquals(
                "\"Shopping\" and \"Travel\" tab groups no longer available",
                model1.get(DESCRIPTION_TEXT));
        assertEquals(MESSAGE, model1.get(CARD_TYPE));

        // Ensure the tab groups are sorted by update time and NOT creation time.
        PropertyModel fooModel = mModelList.get(1).model;
        assertEquals(
                new TabGroupRowViewTitleData("Foo", 2, R.plurals.tab_group_row_accessibility_text),
                fooModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, fooModel.get(COLOR_INDEX));

        PropertyModel barModel = mModelList.get(2).model;
        assertEquals(
                new TabGroupRowViewTitleData("Bar", 3, R.plurals.tab_group_row_accessibility_text),
                barModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, barModel.get(COLOR_INDEX));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabGroupRemovedMessageCardDismissed() {
        List<PersistentMessage> messageList =
                List.of(
                        makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1),
                        makeTabGroupRemovedMessage(MESSAGE_ID2, GROUP_NAME2));
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(messageList);

        SavedTabGroup fooGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        fooGroup.title = "Foo";
        fooGroup.color = TabGroupColorId.BLUE;
        fooGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(2);
        fooGroup.creationTimeMs = 1;
        fooGroup.savedTabs.get(0).updateTimeMs = 2;
        fooGroup.savedTabs.get(1).updateTimeMs = 1;

        SavedTabGroup barGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID2);
        barGroup.title = "Bar";
        barGroup.color = TabGroupColorId.RED;
        barGroup.savedTabs = SyncedGroupTestHelper.tabsFromCount(3);
        barGroup.creationTimeMs = 2;
        barGroup.savedTabs.get(0).updateTimeMs = 3;
        barGroup.savedTabs.get(1).updateTimeMs = 2;
        barGroup.savedTabs.get(2).updateTimeMs = 1;

        createMediator();

        // Dismiss the message card.
        PropertyModel modelToBeRemoved = mModelList.get(0).model;
        modelToBeRemoved.get(UI_DISMISS_ACTION_PROVIDER).action();

        assertEquals(2, mModelList.size());
        verify(mMessagingBackendService)
                .clearPersistentMessage(MESSAGE_ID1, PersistentNotificationType.TOMBSTONED);
        verify(mMessagingBackendService)
                .clearPersistentMessage(MESSAGE_ID2, PersistentNotificationType.TOMBSTONED);

        PropertyModel barModel = mModelList.get(0).model;
        assertEquals(
                new TabGroupRowViewTitleData("Bar", 3, R.plurals.tab_group_row_accessibility_text),
                barModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, barModel.get(COLOR_INDEX));

        // Ensure the tab groups are sorted by update time and NOT creation time.
        PropertyModel fooModel = mModelList.get(1).model;
        assertEquals(
                new TabGroupRowViewTitleData("Foo", 2, R.plurals.tab_group_row_accessibility_text),
                fooModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, fooModel.get(COLOR_INDEX));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSyncObservationForTabGroupRemovedCards() {
        PersistentMessage originalMessage = makeTabGroupRemovedMessage(MESSAGE_ID1, GROUP_NAME1);
        PersistentMessage newMessageCard = makeTabGroupRemovedMessage(MESSAGE_ID2, GROUP_NAME2);

        // Set up backend to return the initial message.
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(List.of(originalMessage));

        createMediator();
        assertEquals(1, mModelList.size());

        // Invoke displayPersistentMessage from backend which adds one more message.
        when(mMessagingBackendService.getMessages(anyInt()))
                .thenReturn(List.of(originalMessage, newMessageCard));
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(newMessageCard);
        ShadowLooper.idleMainLooper();

        assertEquals(1, mModelList.size());

        // Disable sync which should clear all the backend messages.
        when(mSyncService.getActiveDataTypes()).thenReturn(Set.of());
        when(mMessagingBackendService.getMessages(anyInt())).thenReturn(List.of());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testPersistentVersioningMessagePossiblyQueued() {
        createMediator();
        verify(mPersistentVersioningMessageMediator, times(2)).queueMessageIfNeeded();
    }

    private PersistentMessage makeTabGroupRemovedMessage(String messageId, String groupName) {
        PersistentMessage message = new PersistentMessage();
        message.type = PersistentNotificationType.TOMBSTONED;
        message.collaborationEvent = CollaborationEvent.TAB_GROUP_REMOVED;
        message.attribution = new MessageAttribution();
        message.attribution.id = messageId;
        message.attribution.tabMetadata = new TabMessageMetadata();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.lastKnownTitle = groupName;
        return message;
    }
}
