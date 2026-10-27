// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabGroupMenuActionHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabGroupMenuActionHandlerUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private SyncService mSyncService;
    @Mock private Tab mTab;
    @Mock private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private UserPrefs.Natives mUserPrefsJni;

    private TabGroupMenuActionHandler mHandler;
    private TabGroupListBottomSheetCoordinatorFactory mFactory;
    private Context mContext;
    private @Nullable TabGroupCreationCallback mTabGroupCreationCallback;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        CollaborationServiceFactory.setForTesting(mock());
        DataSharingServiceFactory.setForTesting(mock());
        TrackerFactory.setTrackerForTests(mock());
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        when(mSyncService.getActiveDataTypes()).thenReturn(Collections.emptySet());
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        when(mTabModel.containsTabGroup(any())).thenReturn(true);
        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Collections.emptySet());

        mFactory =
                (a, b, callback, d, e, f, g, h, i, j) -> {
                    mTabGroupCreationCallback = callback;
                    return mTabGroupListBottomSheetCoordinator;
                };

        mHandler =
                new TabGroupMenuActionHandler(
                        mContext,
                        mTabModel,
                        mBottomSheetController,
                        mModalDialogManager,
                        mTabGroupUiActionHandler,
                        mTabGroupSyncService,
                        mProfile,
                        mFactory);
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTabModel.getProfile()).thenReturn(mProfile);
    }

    @Test
    public void testHandleAddToGroupAction_noGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
        assertFalse(mHandler.handleAddToGroupAction(mTab));
        verify(mTabModel).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator, never()).showBottomSheet(any());
    }

    @Test
    public void testHandleAddToGroupAction_withGroups() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = "sync_id";
        group.savedTabs = List.of(new SavedTabGroupTab());
        group.localId = new LocalTabGroupId(Token.createRandom());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_id"});
        when(mTabGroupSyncService.getGroup("sync_id")).thenReturn(group);
        assertTrue(mHandler.handleAddToGroupAction(mTab));
        verify(mTabModel, never()).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator).showBottomSheet(any());
    }

    @Test
    public void testHandleAddToGroupAction_onlyInCurrentGroup() {
        Token currentGroupId = Token.createRandom();
        when(mTab.getTabGroupId()).thenReturn(currentGroupId);
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = "sync_id";
        group.savedTabs = List.of(new SavedTabGroupTab());
        group.localId = new LocalTabGroupId(currentGroupId);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_id"});
        when(mTabGroupSyncService.getGroup("sync_id")).thenReturn(group);

        assertFalse(mHandler.handleAddToGroupAction(mTab));
        verify(mTabUngrouper)
                .ungroupTabs(List.of(mTab), /* trailing= */ false, /* allowDialog= */ false);
        verify(mTabModel).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator, never()).showBottomSheet(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS})
    public void testHandleAddToGroupAction_crossWindowGroups() {
        Token otherGroupId = Token.createRandom();
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabModelSelector otherSelector = mock(TabModelSelector.class);
        TabModel otherModel = mock(TabModel.class);
        when(otherSelector.getModel(false)).thenReturn(otherModel);
        when(otherModel.getAllTabGroupIds()).thenReturn(Collections.singleton(otherGroupId));
        when(otherModel.containsTabGroup(otherGroupId)).thenReturn(true);
        when(tabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(otherSelector));
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager));

        TabGroupMenuActionHandler localHandler =
                new TabGroupMenuActionHandler(
                        mContext,
                        mTabModel,
                        mBottomSheetController,
                        mModalDialogManager,
                        mTabGroupUiActionHandler,
                        /* syncService= */ null,
                        mProfile,
                        mFactory);

        assertTrue(localHandler.handleAddToGroupAction(mTab));

        verify(mTabModel, never()).createSingleTabGroup(mTab);
        verify(mTabGroupListBottomSheetCoordinator).showBottomSheet(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> TabWindowManagerSingleton.setTabWindowManagerForTesting(null));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnTabGroupCreation_withCoordinator() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = "sync_id";
        group.savedTabs = List.of(new SavedTabGroupTab());
        group.localId = new LocalTabGroupId(Token.createRandom());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_id"});
        when(mTabGroupSyncService.getGroup("sync_id")).thenReturn(group);
        mHandler.handleAddToGroupAction(mTab);

        assertNotNull(mTabGroupCreationCallback);
        mTabGroupCreationCallback.onTabGroupCreated(Token.createRandom());
        verify(mTabModel, never()).createSingleTabGroup(mTab);
    }

    @Test
    public void testOnTabGroupCreation_noCoordinator() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
        mHandler.handleAddToGroupAction(mTab);

        verify(mTabModel).createSingleTabGroup(mTab);
    }

    @Test
    public void testHandleAddToExistingGroupAction() {
        UserActionTester actionTester = new UserActionTester();
        Token groupId = Token.createRandom();
        Tab destTab = mock(Tab.class);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(destTab));
        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);

        assertTrue(mHandler.handleAddToExistingGroupAction(mTab, groupId, /* syncGroupId= */ null));

        verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        eq(List.of(mTab)),
                        eq(groupId),
                        /* indexInGroup= */ isNull(),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        assertTrue(actionTester.getActions().contains("MobileMenuAddToExistingGroup"));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testHandleAddToExistingGroupAction_remoteGroup() {
        UserActionTester actionTester = new UserActionTester();
        SavedTabGroup remoteGroup = new SavedTabGroup();
        remoteGroup.syncId = "sync_group_id";
        remoteGroup.localId = null;
        remoteGroup.title = "Remote Group";

        when(mTabGroupSyncService.getGroup("sync_group_id")).thenReturn(remoteGroup);

        assertTrue(
                mHandler.handleAddToExistingGroupAction(
                        mTab, /* groupId= */ null, "sync_group_id"));

        verify(mTabGroupUiActionHandler).openTabGroup("sync_group_id");
        assertTrue(actionTester.getActions().contains("MobileMenuAddToExistingGroup"));
    }

    @Test
    public void testHandleAddToExistingGroupAction_invalidDestination() {
        assertFalse(
                mHandler.handleAddToExistingGroupAction(
                        mTab, /* groupId= */ null, /* syncGroupId= */ null));
    }

    @Test
    public void testHandleOpenGroupAction_nullSyncGroupId() {
        UserActionTester actionTester = new UserActionTester();
        assertFalse(mHandler.handleOpenGroupAction(/* syncGroupId= */ null));
        verify(mTabGroupUiActionHandler, never()).openTabGroup(any());
        assertFalse(actionTester.getActions().contains("MobileMenuOpenGroup"));
    }

    @Test
    public void testHandleOpenGroupAction_validSyncGroupId() {
        UserActionTester actionTester = new UserActionTester();
        assertTrue(mHandler.handleOpenGroupAction("sync_group_id"));
        verify(mTabGroupUiActionHandler).openTabGroup("sync_group_id");
        assertTrue(actionTester.getActions().contains("MobileMenuOpenGroup"));
    }

    @Test
    public void testHandleOpenGroupInNewWindowAction_groupNotFound() {
        UserActionTester actionTester = new UserActionTester();
        MultiInstanceManager multiInstanceManager = mock(MultiInstanceManager.class);
        assertFalse(
                mHandler.handleOpenGroupInNewWindowAction(
                        /* groupId= */ null, /* syncGroupId= */ null, multiInstanceManager));
        assertFalse(actionTester.getActions().contains("MobileMenuOpenGroupInNewWindow"));
    }

    @Test
    public void testHandleOpenGroupInNewWindowAction_success() {
        UserActionTester actionTester = new UserActionTester();
        MultiInstanceManager multiInstanceManager = mock(MultiInstanceManager.class);
        MultiInstanceOrchestrator orchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(orchestrator);

        Token groupId = Token.createRandom();
        Tab groupTab = mock(Tab.class);
        when(groupTab.getId()).thenReturn(1);
        when(groupTab.getTabGroupId()).thenReturn(groupId);
        when(groupTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(groupTab));

        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        when(tabWindowManager.findWindowIdForTabGroup(groupId)).thenReturn(0);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        assertTrue(
                mHandler.handleOpenGroupInNewWindowAction(
                        groupId, /* syncGroupId= */ null, multiInstanceManager));

        verify(orchestrator)
                .moveTabGroupToNewWindow(
                        any(TabGroupMetadata.class),
                        eq(MultiInstanceManager.NewWindowAppSource.MENU));
        verify(multiInstanceManager).closeChromeWindowIfEmpty(0);
        assertTrue(actionTester.getActions().contains("MobileMenuOpenGroupInNewWindow"));
    }

    @Test
    public void testHandleDeleteGroupAction_groupNotFound() {
        UserActionTester actionTester = new UserActionTester();
        assertFalse(mHandler.handleDeleteGroupAction(/* groupId= */ null, /* syncGroupId= */ null));
        assertFalse(actionTester.getActions().contains("MobileMenuDeleteGroup"));
    }

    @Test
    public void testHandleDeleteGroupAction_inCurrent_noConfirmationNeeded() {
        UserActionTester actionTester = new UserActionTester();
        Token groupId = Token.createRandom();
        Tab groupTab = mock(Tab.class);
        TabRemover tabRemover = mock(TabRemover.class);
        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(groupTab));
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);

        assertTrue(mHandler.handleDeleteGroupAction(groupId, /* syncGroupId= */ null));

        verify(tabRemover).closeTabs(any(), eq(true));
        assertTrue(actionTester.getActions().contains("MobileMenuDeleteGroup"));
    }

    @Test
    public void testHandleDeleteGroupAction_hidden_triggersConfirmation() {
        UserActionTester actionTester = new UserActionTester();
        SavedTabGroup hiddenGroup = new SavedTabGroup();
        hiddenGroup.syncId = "hidden_sync_id";
        hiddenGroup.localId = null;
        when(mTabGroupSyncService.getGroup("hidden_sync_id")).thenReturn(hiddenGroup);

        assertTrue(mHandler.handleDeleteGroupAction(/* groupId= */ null, "hidden_sync_id"));

        verify(mModalDialogManager).showDialog(any(), anyInt());
        assertTrue(actionTester.getActions().contains("MobileMenuDeleteGroup"));
    }
}
