// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabSelectionEditorActionProvider}.
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorActionProviderUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabSelectionEditorCoordinator.TabSelectionEditorController mTabSelectionEditorController;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;

    private TabImpl mTab1;
    private TabImpl mTab2;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
    }

    @Test
    public void testGroupAction() {
        TabSelectionEditorActionProvider tabSelectionEditorActionProvider =
                new TabSelectionEditorActionProvider(mTabSelectionEditorController,
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP);
        List<Tab> selectedTabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        tabSelectionEditorActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);

        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(eq(selectedTabs), eq(mTab2), eq(false), eq(true));
        verify(mTabSelectionEditorController).hide();
    }

    @Test
    public void testUngroupAction() {
        TabSelectionEditorActionProvider tabSelectionEditorActionProvider =
                new TabSelectionEditorActionProvider(mTabSelectionEditorController,
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.UNGROUP);
        List<Tab> selectedTabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        tabSelectionEditorActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(TAB1_ID);
        verify(mTabGroupModelFilter).moveTabOutOfGroup(TAB2_ID);
        verify(mTabSelectionEditorController).hide();
    }

    @Test
    public void testCloseAction() {
        TabSelectionEditorActionProvider tabSelectionEditorActionProvider =
                new TabSelectionEditorActionProvider(mTabSelectionEditorController,
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.CLOSE);
        List<Tab> selectedTabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        tabSelectionEditorActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);

        verify(mTabModel).closeMultipleTabs(selectedTabs, true);
        verify(mTabSelectionEditorController).hide();
    }

    @Test
    public void testCustomizeAction() {
        AtomicBoolean isProcessed = new AtomicBoolean();

        TabSelectionEditorActionProvider tabSelectionEditorActionProvider =
                new TabSelectionEditorActionProvider() {
                    @Override
                    void processSelectedTabs(
                            List<Tab> selectedTabs, TabModelSelector tabModelSelector) {
                        isProcessed.set(true);
                    }
                };
        List<Tab> selectedTabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        tabSelectionEditorActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);

        assertTrue(isProcessed.get());
    }

    private TabImpl prepareTab(int id, String title) {
        TabImpl tab = mock(TabImpl.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn(GURL.emptyGURL()).when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(false).when(tab).isClosing();
        return tab;
    }
}
