// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.core.util.Pair;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link TabGroupListMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.DATA_SHARING)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
public class TabGroupListMediatorUnitTest {
    private static final String SYNC_GROUP_ID1 = "remote one";
    private static final String SYNC_GROUP_ID2 = "remote two";
    private static final String SYNC_GROUP_ID3 = "remote three";
    private static final String COLLABORATION_ID1 = "A";
    private static final String GAIA_ID1 = "Z";
    private static final String GAIA_ID2 = "Y";
    private static final String EMAIL = "fake@gmail.com";
    private static final Token LOCAL_GROUP_ID1 = new Token(1, 1);
    private static final Token LOCAL_GROUP_ID2 = new Token(2, 2);

    private static final int ROOT_ID1 = 1;
    private static final int ROOT_ID2 = 2;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabList mComprehensiveModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PaneManager mPaneManager;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private TabSwitcherPaneBase mTabSwitcherPaneBase;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private SyncService mSyncService;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private ModalDialogManager mModalDialogManager;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserver;
    @Captor private ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mConfirmationResultCallbackCaptor;

    @Captor
    private ArgumentCaptor<SyncService.SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mActionOutcomeCallbackCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mModalPropertyModelCaptor;

    private PropertyModel mPropertyModel;
    private ModelList mModelList;
    private Context mContext;

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
    }

    private TabGroupListMediator createMediator() {
        return new TabGroupListMediator(
                mContext,
                mModelList,
                mPropertyModel,
                mTabGroupModelFilter,
                mFaviconResolver,
                mTabGroupSyncService,
                mDataSharingService,
                mIdentityManager,
                mPaneManager,
                mTabGroupUiActionHandler,
                mActionConfirmationManager,
                mSyncService,
                mModalDialogManager);
    }

    @Test
    public void testNoTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
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
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        createMediator();
        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals(new Pair<>("Title", 1), model.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, model.get(COLOR_INDEX));
    }

    @Test
    public void testTwoGroups() {
        SavedTabGroup fooGroup = new SavedTabGroup();
        fooGroup.syncId = SYNC_GROUP_ID1;
        fooGroup.title = "Foo";
        fooGroup.color = TabGroupColorId.BLUE;
        fooGroup.savedTabs = Arrays.asList(new SavedTabGroupTab(), new SavedTabGroupTab());
        fooGroup.creationTimeMs = 1;

        SavedTabGroup barGroup = new SavedTabGroup();
        barGroup.syncId = SYNC_GROUP_ID1;
        barGroup.title = "Bar";
        barGroup.color = TabGroupColorId.RED;
        barGroup.savedTabs =
                Arrays.asList(
                        new SavedTabGroupTab(), new SavedTabGroupTab(), new SavedTabGroupTab());
        barGroup.creationTimeMs = 2;

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(fooGroup);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(barGroup);

        createMediator();
        assertEquals(2, mModelList.size());

        PropertyModel barModel = mModelList.get(0).model;
        assertEquals(new Pair<>("Bar", 3), barModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, barModel.get(COLOR_INDEX));

        PropertyModel fooModel = mModelList.get(1).model;
        assertEquals(new Pair<>("Foo", 2), fooModel.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, fooModel.get(COLOR_INDEX));
    }

    @Test
    public void testSyncObservation() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        createMediator();
        assertEquals(1, mModelList.size());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        mTabGroupSyncObserverCaptor
                .getValue()
                .onTabGroupRemoved(SYNC_GROUP_ID1, TriggerSource.LOCAL);
        ShadowLooper.idleMainLooper();

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testTabModelObservation() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        createMediator();
        assertEquals(1, mModelList.size());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        verify(mTabGroupModelFilter).addObserver(mTabModelObserver.capture());
        mTabModelObserver.getValue().tabClosureUndone(mTab1);
        ShadowLooper.idleMainLooper();

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testFilterOutOtherTabGroups() {
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.title = "in current";
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = new SavedTabGroup();
        group2.syncId = SYNC_GROUP_ID2;
        group2.title = "in another";
        group2.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group2.localId = new LocalTabGroupId(LOCAL_GROUP_ID2);

        SavedTabGroup group3 = new SavedTabGroup();
        group3.syncId = SYNC_GROUP_ID3;
        group3.title = "hidden";
        group3.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group3.localId = null;

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2, SYNC_GROUP_ID3});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(group2);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID3)).thenReturn(group3);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;
        assertEquals(new Pair<>("in current", 1), model1.get(TITLE_DATA));
        PropertyModel model2 = mModelList.get(1).model;
        assertEquals(new Pair<>("hidden", 1), model2.get(TITLE_DATA));
    }

    @Test
    public void testOpenRunnable() {
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = new SavedTabGroup();
        group2.syncId = SYNC_GROUP_ID2;
        group2.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group2.localId = null;

        SavedTabGroup updatedGroup2 = new SavedTabGroup();
        updatedGroup2.syncId = SYNC_GROUP_ID2;
        updatedGroup2.savedTabs = Arrays.asList(new SavedTabGroupTab());
        updatedGroup2.localId = new LocalTabGroupId(LOCAL_GROUP_ID2);

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(group2);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
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
                            when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2))
                                    .thenReturn(updatedGroup2);
                            when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID2))
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
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = ROOT_ID1;
        group1.savedTabs = Arrays.asList(savedTab);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        // Pretend the closure was already committed so we need to fallback to the HIDDEN behavior.
        group1.localId = null;
        when(mComprehensiveModel.getCount()).thenReturn(0);
        MockitoHelper.doRunnable(
                        () -> {
                            group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
                            when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1))
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
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = ROOT_ID1;
        group1.savedTabs = Arrays.asList(savedTab);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(OPEN_RUNNABLE).run();
        verify(mTabModel).cancelTabClosure(ROOT_ID1);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(ROOT_ID1);
    }

    @Test
    public void testOpenRunnable_ClosingAfterShowing() {
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = ROOT_ID1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = ROOT_ID2;

        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(savedTab1, savedTab2);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mComprehensiveModel.getCount()).thenReturn(2);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mComprehensiveModel.getTabAt(1)).thenReturn(mTab2);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab2.getRootId()).thenReturn(ROOT_ID1);
        when(mTab2.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);

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
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        SavedTabGroup group2 = new SavedTabGroup();
        group2.syncId = SYNC_GROUP_ID2;
        group2.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group2.localId = null;

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(group2);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID2))
                .thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1))
                .thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;
        model1.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupModelFilter).closeTabs(any());

        reset(mActionConfirmationManager);
        PropertyModel model2 = mModelList.get(1).model;
        model2.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID2);
    }

    @Test
    public void testDeleteRunnable_NoConfirmation() {
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1))
                .thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mTabGroupSyncService, never()).removeGroup(anyString());
    }

    @Test
    public void testDeleteRunnable_CurrentClosing() {
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = ROOT_ID1;
        group1.savedTabs = Arrays.asList(savedTab);
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1))
                .thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(true);

        createMediator();

        assertEquals(1, mModelList.size());

        PropertyModel model1 = mModelList.get(0).model;
        model1.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabModel).commitTabClosure(ROOT_ID1);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID1);
    }

    @Test
    public void testEmptyStateEnabled() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        createMediator();
        assertTrue(mPropertyModel.get(TabGroupListProperties.EMPTY_STATE_VISIBLE));

        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);
        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        mTabGroupSyncObserverCaptor
                .getValue()
                .onTabGroupRemoved(SYNC_GROUP_ID1, TriggerSource.LOCAL);
        ShadowLooper.idleMainLooper();
        assertFalse(mPropertyModel.get(TabGroupListProperties.EMPTY_STATE_VISIBLE));
    }

    @Test
    public void testSyncEnabled() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
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
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        createMediator().destroy();

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupSyncService).removeObserver(any());
        verify(mSyncService).removeSyncStateChangedListener(any());

        verify(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());
        reset(mTabGroupSyncService);
        mTabGroupSyncObserverCaptor.getValue().onTabGroupAdded(null, 0);
        ShadowLooper.idleMainLooper();
        verify(mTabGroupSyncService, never()).getAllGroupIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testDeleteRunnable_SharedGroup() {
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1))
                .thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;
        assertNull(model.get(DELETE_RUNNABLE));

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());

        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertNotNull(model.get(DELETE_RUNNABLE));
        model.get(DELETE_RUNNABLE).run();

        verify(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(
                        any(), mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);

        verify(mDataSharingService)
                .deleteGroup(eq(COLLABORATION_ID1), mActionOutcomeCallbackCaptor.capture());
        mActionOutcomeCallbackCaptor.getValue().onResult(PeopleGroupActionOutcome.SUCCESS);
        verify(mModalDialogManager, never())
                .showDialog(mModalPropertyModelCaptor.capture(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testLeaveRunnable() {
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = List.of(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1)).thenReturn(List.of(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;
        assertNull(model.get(LEAVE_RUNNABLE));

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());

        GroupMember groupMember1 =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.MEMBER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember groupMember2 =
                new GroupMember(
                        GAIA_ID2,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember1, groupMember2};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertNotNull(model.get(LEAVE_RUNNABLE));
        model.get(LEAVE_RUNNABLE).run();

        verify(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);

        verify(mDataSharingService)
                .removeMember(
                        eq(COLLABORATION_ID1), eq(EMAIL), mActionOutcomeCallbackCaptor.capture());
        mActionOutcomeCallbackCaptor
                .getValue()
                .onResult(PeopleGroupActionOutcome.TRANSIENT_FAILURE);

        verify(mModalDialogManager).showDialog(mModalPropertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                mModalPropertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mModalPropertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testDeleteRunnable_shareReadFailure() {
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab());
        group1.localId = new LocalTabGroupId(LOCAL_GROUP_ID1);
        group1.collaborationId = COLLABORATION_ID1;

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupModelFilter.getRootIdFromStableId(LOCAL_GROUP_ID1)).thenReturn(ROOT_ID1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID1))
                .thenReturn(Arrays.asList(mTab1));
        when(mComprehensiveModel.getCount()).thenReturn(1);
        when(mComprehensiveModel.getTabAt(0)).thenReturn(mTab1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID1);
        when(mTab1.getTabGroupId()).thenReturn(LOCAL_GROUP_ID1);
        when(mTab1.isClosing()).thenReturn(false);

        createMediator();

        assertEquals(1, mModelList.size());
        PropertyModel model = mModelList.get(0).model;
        assertNull(model.get(DELETE_RUNNABLE));

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());

        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(
                        /* groupData= */ null, PeopleGroupActionFailure.TRANSIENT_FAILURE);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertNotNull(model.get(DELETE_RUNNABLE));
        model.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager).processDeleteGroupAttempt(any());
    }
}
