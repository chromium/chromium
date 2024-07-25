// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;

import java.util.List;

/** Unit tests for {@link TabUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiUtilsUnitTest {
    private static final int TAB_ID = 123;
    private static final int ROOT_ID = TAB_ID;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private Tab mTab;

    private List<Tab> mTabsToClose;

    @Before
    public void setUp() {
        mTabsToClose = List.of(mTab);
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mFilter.isIncognitoBranded()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getRootId()).thenReturn(ROOT_ID);
        when(mFilter.getRelatedTabListForRootId(ROOT_ID)).thenReturn(mTabsToClose);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.isClosing()).thenReturn(false);
        when(mTab.getId()).thenReturn(TAB_ID);
    }

    @Test
    public void testCloseTabGroup_Incognito() {
        boolean hideTabGroups = false;
        when(mFilter.isIncognitoBranded()).thenReturn(true);

        TabUiUtils.closeTabGroup(mFilter, mActionConfirmationManager, TAB_ID, hideTabGroups);

        verify(mFilter).closeMultipleTabs(mTabsToClose, /* canUndo= */ true, hideTabGroups);
    }

    @Test
    public void testCloseTabGroup_Hide() {
        boolean hideTabGroups = true;

        TabUiUtils.closeTabGroup(mFilter, mActionConfirmationManager, TAB_ID, hideTabGroups);

        verify(mFilter).closeMultipleTabs(mTabsToClose, /* canUndo= */ true, hideTabGroups);
    }

    @Test
    public void testCloseTabGroup_Delete_Positive() {
        boolean hideTabGroups = false;
        doCallback(
                        0,
                        (Callback<Integer> resultCallback) -> {
                            resultCallback.onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
                        })
                .when(mActionConfirmationManager)
                .processDeleteGroupAttempt(any());

        TabUiUtils.closeTabGroup(mFilter, mActionConfirmationManager, TAB_ID, hideTabGroups);

        verify(mActionConfirmationManager).processDeleteGroupAttempt(any());
        verify(mFilter).closeMultipleTabs(mTabsToClose, /* canUndo= */ false, hideTabGroups);
    }

    @Test
    public void testCloseTabGroup_Delete_Positive_Immediate() {
        boolean hideTabGroups = false;
        doCallback(
                        0,
                        (Callback<Integer> resultCallback) -> {
                            resultCallback.onResult(ConfirmationResult.IMMEDIATE_CONTINUE);
                        })
                .when(mActionConfirmationManager)
                .processDeleteGroupAttempt(any());

        TabUiUtils.closeTabGroup(mFilter, mActionConfirmationManager, TAB_ID, hideTabGroups);

        verify(mActionConfirmationManager).processDeleteGroupAttempt(any());
        verify(mFilter).closeMultipleTabs(mTabsToClose, /* canUndo= */ true, hideTabGroups);
    }

    @Test
    public void testCloseTabGroup_Delete_Negative() {
        boolean hideTabGroups = false;
        doCallback(
                        0,
                        (Callback<Integer> resultCallback) -> {
                            resultCallback.onResult(ConfirmationResult.CONFIRMATION_NEGATIVE);
                        })
                .when(mActionConfirmationManager)
                .processDeleteGroupAttempt(any());

        TabUiUtils.closeTabGroup(mFilter, mActionConfirmationManager, TAB_ID, hideTabGroups);

        verify(mActionConfirmationManager).processDeleteGroupAttempt(any());
        verify(mFilter, never()).closeMultipleTabs(any(), anyBoolean(), anyBoolean());
    }
}
