// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;

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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Unit tests for {@link TabUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiUtilsUnitTest {
    private static final int TAB_ID = 123;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tab mTab;
    @Mock private CollaborationService mCollaborationService;
    @Mock private Callback<Boolean> mDidCloseTabsCallback;
    @Mock private Callback<Boolean> mContentSensitivitySetter;
    @Mock private Runnable mFinishBlocking;

    @Captor private ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mOutcomeCaptor;

    @Before
    public void setUp() {
        List<Tab> tabsToClose = List.of(mTab);

        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabsToClose);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);
    }

    @Test
    public void testCloseTabGroup_NoTab() {
        TabUiUtils.closeTabGroup(
                mTabModel,
                Tab.INVALID_TAB_ID,
                TabClosingSource.UNKNOWN,
                /* allowUndo= */ true,
                /* hideTabGroups= */ false,
                mDidCloseTabsCallback);
        verify(mDidCloseTabsCallback).onResult(false);
    }

    @Test
    public void testCloseTabGroup_AllowUndo() {
        testCloseTabGroupForAllowUndoParam(/* shouldAllowUndo= */ true);
    }

    @Test
    public void testCloseTabGroup_DisallowUndo() {
        testCloseTabGroupForAllowUndoParam(/* shouldAllowUndo= */ false);
    }

    private void testCloseTabGroupForAllowUndoParam(boolean shouldAllowUndo) {
        // Act
        TabUiUtils.closeTabGroup(
                mTabModel,
                TAB_ID,
                TabClosingSource.UNKNOWN,
                shouldAllowUndo,
                /* hideTabGroups= */ false,
                /* didCloseCallback= */ null);

        // Assert
        ArgumentCaptor<TabClosureParams> tabClosureParamsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mTabRemover)
                .closeTabs(
                        tabClosureParamsCaptor.capture(),
                        /* allowDialog= */ anyBoolean(),
                        /* listener= */ nullable(TabModelActionListener.class));
        assertEquals(shouldAllowUndo, tabClosureParamsCaptor.getValue().allowUndo);
    }

    @Test
    public void testCloseTabGroup_NoHide() {
        boolean hideTabGroups = false;

        TabUiUtils.closeTabGroup(
                mTabModel,
                TAB_ID,
                TabClosingSource.TABLET_TAB_STRIP,
                /* allowUndo= */ true,
                hideTabGroups,
                mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
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

        TabUiUtils.closeTabGroup(
                mTabModel,
                TAB_ID,
                TabClosingSource.TABLET_TAB_STRIP,
                /* allowUndo= */ true,
                hideTabGroups,
                mDidCloseTabsCallback);

        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID)
                                        .hideTabGroups(hideTabGroups)
                                        .allowUndo(true)
                                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                        .build()),
                        eq(true),
                        mTabModelActionListenerCaptor.capture());
    }

    @Test
    public void testExitCollaborationWithoutWarning_Owner() {
        TabUiUtils.exitCollaborationWithoutWarning(
                ApplicationProvider.getApplicationContext(),
                mModalDialogManager,
                mCollaborationService,
                COLLABORATION_ID1,
                MemberRole.OWNER,
                mFinishBlocking);
        verify(mCollaborationService).deleteGroup(eq(COLLABORATION_ID1), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(false);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();
    }

    @Test
    public void testExitCollaborationWithoutWarning_Member() {
        TabUiUtils.exitCollaborationWithoutWarning(
                ApplicationProvider.getApplicationContext(),
                mModalDialogManager,
                mCollaborationService,
                COLLABORATION_ID1,
                MemberRole.MEMBER,
                mFinishBlocking);
        verify(mCollaborationService).leaveGroup(eq(COLLABORATION_ID1), mOutcomeCaptor.capture());

        mOutcomeCaptor.getValue().onResult(false);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();
    }

    @Test
    public void testUpdateViewContentSensitivityForListOfTabs() {
        List<Tab> tabList = List.of(mTab);
        final String histogram = "SensitiveContent.TabSwitching.RegularTabSwitcherPane.Sensitivity";

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                tabList, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }

    @Test
    public void testUpdateViewContentSensitivityForTabList() {
        final String histogram = "SensitiveContent.TabSwitching.BottomTabStripGroupUI.Sensitivity";

        List<Tab> tabList = List.of(mTab);
        when(mTabModel.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTabModel.getCount()).thenAnswer(invocation -> 1);
        when(mTabModel.getTabAtChecked(0)).thenAnswer(invocation -> mTab);

        HistogramWatcher histogramWatcherForTrueBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ true);
        when(mTab.getTabHasSensitiveContent()).thenReturn(true);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(true);
        histogramWatcherForTrueBucket.assertExpected();

        HistogramWatcher histogramWatcherForFalseBucket =
                HistogramWatcher.newSingleRecordWatcher(histogram, /* value= */ false);
        when(mTab.getTabHasSensitiveContent()).thenReturn(false);
        TabUiUtils.updateViewContentSensitivityForTabs(
                mTabModel, mContentSensitivitySetter, histogram);
        verify(mContentSensitivitySetter).onResult(false);
        histogramWatcherForFalseBucket.assertExpected();
    }

    @Test
    public void testUpdateTabGroupColor() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupColor(TAB_GROUP_ID)).thenReturn(TabGroupColorId.BLUE);
        TabUiUtils.updateTabGroupColor(mTabModel, TAB_GROUP_ID, TabGroupColorId.RED);
        verify(mTabModel).setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.RED);

        TabUiUtils.updateTabGroupColor(mTabModel, TAB_GROUP_ID, TabGroupColorId.BLUE);
        verify(mTabModel, never()).setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        TabUiUtils.updateTabGroupColor(mTabModel, TAB_GROUP_ID, TabGroupColorId.YELLOW);
        verify(mTabModel, never()).setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.YELLOW);
    }

    @Test
    public void testUpdateTabGroupTitle() {
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("B");
        TabUiUtils.updateTabGroupTitle(mTabModel, TAB_GROUP_ID, "A");
        verify(mTabModel).setTabGroupTitle(TAB_GROUP_ID, "A");

        TabUiUtils.updateTabGroupTitle(mTabModel, TAB_GROUP_ID, "B");
        verify(mTabModel, never()).setTabGroupTitle(TAB_GROUP_ID, "B");

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        TabUiUtils.updateTabGroupTitle(mTabModel, TAB_GROUP_ID, "C");
        verify(mTabModel, never()).setTabGroupTitle(TAB_GROUP_ID, "C");

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("A");
        TabUiUtils.updateTabGroupTitle(mTabModel, TAB_GROUP_ID, UNSET_TAB_GROUP_TITLE);
        verify(mTabModel).setTabGroupTitle(TAB_GROUP_ID, UNSET_TAB_GROUP_TITLE);
    }
}
