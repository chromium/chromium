// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

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

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Unit tests for {@link TabUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabUiUtilsUnitTest {
    private static final int TAB_ID = 123;
    private static final int ROOT_ID = TAB_ID;
    private static final String GROUP_TITLE = "My Group";
    private static final String COLLABORATION_ID1 = "A";
    private static final String GAIA_ID = "Z";
    private static final String EMAIL = "fake@gmail.com";
    private static final Token TAB_GROUP_TOKEN = Token.createRandom();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabRemover mTabRemover;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private Callback<Boolean> mDidCloseTabsCallback;
    @Mock private Callback<Boolean> mContentSensitivitySetter;

    @Captor private ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOutcomeCaptor;

    private List<Tab> mTabsToClose;

    @Before
    public void setUp() {
        mTabsToClose = List.of(mTab);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mFilter.isIncognitoBranded()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getRootId()).thenReturn(ROOT_ID);
        when(mFilter.getRelatedTabListForRootId(ROOT_ID)).thenReturn(mTabsToClose);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.isClosing()).thenReturn(false);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
    }

    @Test
    public void testCloseTabGroup_NoHide() {
        boolean hideTabGroups = false;

        TabUiUtils.closeTabGroup(mFilter, TAB_ID, hideTabGroups, mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mFilter, ROOT_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .build()),
                        eq(true),
                        mTabModelActionListenerCaptor.capture());

        // These are the known valid combinations only:
        TabModelActionListener listener = mTabModelActionListenerCaptor.getValue();

        listener.onConfirmationDialogResult(
                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mDidCloseTabsCallback).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mDidCloseTabsCallback, times(2)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mDidCloseTabsCallback, times(3)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mDidCloseTabsCallback).onResult(false);

        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mDidCloseTabsCallback, times(4)).onResult(true);

        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mDidCloseTabsCallback, times(5)).onResult(true);
    }

    @Test
    public void testCloseTabGroup_Hide() {
        boolean hideTabGroups = true;

        TabUiUtils.closeTabGroup(mFilter, TAB_ID, hideTabGroups, mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mFilter, ROOT_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .build()),
                        eq(true),
                        mTabModelActionListenerCaptor.capture());
    }

    @Test
    public void testDeleteSharedTabGroup_Positive() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processDeleteSharedGroupAttempt(eq(GROUP_TITLE), any());
        verify(mDataSharingService).deleteGroup(eq(COLLABORATION_ID1), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(PeopleGroupActionOutcome.TRANSIENT_FAILURE);
        verify(mModalDialogManager).showDialog(any(), anyInt());
    }

    @Test
    public void testDeleteSharedTabGroup_Negative() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_NEGATIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processDeleteSharedGroupAttempt(eq(GROUP_TITLE), any());
        verify(mDataSharingService, never()).deleteGroup(any(), any());
    }

    @Test
    public void testDeleteSharedTabGroup_NullTab() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
    }

    @Test
    public void testDeleteSharedTabGroup_NullTabGroupId() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        when(mTab.getTabGroupId()).thenReturn(null);
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
    }

    @Test
    public void testDeleteSharedTabGroup_NullSavedTabGroup() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
    }

    @Test
    public void testDeleteSharedTabGroup_NullCollaborationId() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processDeleteSharedGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        TabUiUtils.deleteSharedTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processDeleteSharedGroupAttempt(any(), any());
    }

    @Test
    public void testLeaveTabGroup_Positive() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        TabUiUtils.leaveTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processLeaveGroupAttempt(eq(GROUP_TITLE), any());
        verify(mDataSharingService)
                .removeMember(eq(COLLABORATION_ID1), eq(EMAIL), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(PeopleGroupActionOutcome.TRANSIENT_FAILURE);
        verify(mModalDialogManager).showDialog(any(), anyInt());
    }

    @Test
    public void testLeaveTabGroup_Negative() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_NEGATIVE))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        TabUiUtils.leaveTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager).processLeaveGroupAttempt(eq(GROUP_TITLE), any());
        verify(mDataSharingService, never()).removeMember(any(), any(), any());
    }

    @Test
    public void testLeaveTabGroup_NullTab() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());

        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        TabUiUtils.leaveTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
    }

    @Test
    public void testLeaveTabGroup_NullSavedTabGroup() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());

        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        TabUiUtils.leaveTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
    }

    @Test
    public void testLeaveTabGroup_NullCoreAccountInfo() {
        doCallback(
                        1,
                        (Callback<Integer> resultCallback) ->
                                resultCallback.onResult(
                                        ActionConfirmationResult.CONFIRMATION_POSITIVE))
                .when(mActionConfirmationManager)
                .processLeaveGroupAttempt(any(), any());

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(null);

        TabUiUtils.leaveTabGroup(
                ApplicationProvider.getApplicationContext(),
                mFilter,
                mActionConfirmationManager,
                mModalDialogManager,
                TAB_ID);
        verify(mActionConfirmationManager, never()).processLeaveGroupAttempt(any(), any());
    }

    @Test
    public void testUpdateViewContentSensitivityForListOfTabs() {
        List<Tab> tabList = List.of(mTab);
        final String histogram = "SensitiveContent.TabSwitching.RegularTabSwitcherPane.Sensitivity";

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* contentIsSensitive= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* contentIsSensitive= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }

    @Test
    public void testUpdateViewContentSensitivityForTabList() {
        final String histogram = "SensitiveContent.TabSwitching.BottomTabStripGroupUI.Sensitivity";

        when(mTabModel.getCount()).thenAnswer(invocation -> 1);
        when(mTabModel.getTabAt(0)).thenAnswer(invocation -> mTab);

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* contentIsSensitive= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* contentIsSensitive= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }
}
