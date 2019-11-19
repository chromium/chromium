// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link TabSuggestionMessageService}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class TabSuggestionMessageServiceUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;

    private static final int CLOSE_SUGGESTION_ACTION_BUTTON_RESOURCE_ID =
            org.chromium.chrome.tab_ui.R.string.tab_suggestion_close_tab_action_button;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    private TabSuggestionMessageService mMessageService;

    @Mock
    Context mContext;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabSelectionEditorCoordinator.TabSelectionEditorController mTabSelectionEditorController;

    @Before
    public void setUp() {
        // After setUp there are three tabs in TabModel.
        MockitoAnnotations.initMocks(this);

        // Set up Tabs.
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_ROOT_ID);

        // Set up TabModelSelector.
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(mTab3).when(mTabModelSelector).getTabById(TAB3_ID);

        // Set up TabModel.
        doReturn(3).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab3).when(mTabModel).getTabAt(POSITION3);

        // Set up TabModelFilter.
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();

        mMessageService = new TabSuggestionMessageService(
                mContext, mTabModelSelector, mTabSelectionEditorController);
    }

    @Test
    public void testReviewHandler_closeSuggestion() {
        prepareTabSuggestions(Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.CLOSE);
        String closeSuggestionActionButtonText = "close";
        int expectedEnablingThreshold =
                TabSuggestionMessageService.CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD;
        doReturn(closeSuggestionActionButtonText)
                .when(mContext)
                .getString(eq(CLOSE_SUGGESTION_ACTION_BUTTON_RESOURCE_ID));

        mMessageService.review();
        verify(mTabSelectionEditorController)
                .configureToolbar(eq(closeSuggestionActionButtonText), any(),
                        eq(expectedEnablingThreshold), any());
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab2, mTab3)), eq(2));

        prepareTabSuggestions(Arrays.asList(mTab1, mTab3), TabSuggestion.TabSuggestionAction.CLOSE);
        mMessageService.review();
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab3, mTab2)), eq(2));
    }

    @Test
    public void testClosingSuggestionActionHandler() {
        List<Tab> suggestedTabs = Arrays.asList(mTab1, mTab2);
        List<TabSuggestion> tabSuggestions =
                prepareTabSuggestions(suggestedTabs, TabSuggestion.TabSuggestionAction.CLOSE);
        TabSuggestion bestSuggestion = tabSuggestions.get(0);

        TabSelectionEditorActionProvider actionProvider =
                mMessageService.getActionProvider(bestSuggestion);
        actionProvider.processSelectedTabs(suggestedTabs, mTabModelSelector);

        verify(mTabModel).closeMultipleTabs(eq(suggestedTabs), eq(true));
        // TODO(crbug.com/1023699): verify callback is called.
    }

    @Test(expected = AssertionError.class)
    public void testInvalidatedSuggestion() {
        mMessageService.onTabSuggestionInvalidated();
        mMessageService.review();
    }

    private List<TabSuggestion> prepareTabSuggestions(
            List<Tab> suggestedTab, @TabSuggestion.TabSuggestionAction int actionCode) {
        List<TabContext.TabInfo> suggestedTabInfo = new ArrayList<>();
        for (int i = 0; i < suggestedTab.size(); i++) {
            TabContext.TabInfo tabInfo = TabContext.TabInfo.createFromTab(suggestedTab.get(i));
            suggestedTabInfo.add(tabInfo);
        }

        TabSuggestion suggestion = new TabSuggestion(suggestedTabInfo, actionCode, "");
        List<TabSuggestion> tabSuggestions =
                Collections.unmodifiableList(Arrays.asList(suggestion));
        mMessageService.onNewSuggestion(tabSuggestions);

        return tabSuggestions;
    }
}