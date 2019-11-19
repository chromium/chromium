// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.LinkedList;
import java.util.List;

/**
 * Tests functionality of {@link TabSuggestionsOrchestrator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSuggestionsOrchestratorTest {
    private static final int[] TAB_IDS = {0, 1, 2};

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private TabModelFilter mTabModelFilter;

    @Mock
    private TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;

    @Mock
    private ActivityLifecycleDispatcher mDispatcher;

    private static Tab[] sTabs = {mockTab(TAB_IDS[0]), mockTab(TAB_IDS[1]), mockTab(TAB_IDS[2])};

    private static Tab mockTab(int id) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        return tab;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(any(TabModelObserver.class));
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(null).when(mTabModelFilter).getRelatedTabList(anyInt());
    }

    @Test
    public void verifyResultsPrefetched() {
        doReturn(TAB_IDS.length).when(mTabModelFilter).getCount();
        for (int idx = 0; idx < TAB_IDS.length; idx++) {
            doReturn(sTabs[idx]).when(mTabModelFilter).getTabAt(eq(idx));
        }
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(mTabModelSelector, mDispatcher);
        List<TabSuggestion> suggestions = new LinkedList<>();
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions) {
                suggestions.addAll(tabSuggestions);
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };
        tabSuggestionsOrchestrator.addObserver(tabSuggestionsObserver);
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(null, 0);
        Assert.assertEquals(1, suggestions.size());
        Assert.assertEquals(TAB_IDS.length, suggestions.get(0).getTabsInfo().size());
        for (int idx = 0; idx < TAB_IDS.length; idx++) {
            Assert.assertEquals(TAB_IDS[idx], suggestions.get(0).getTabsInfo().get(idx).id);
        }
    }

    @Test
    public void testRegisterUnregister() {
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(mTabModelSelector, mDispatcher);
        verify(mDispatcher, times(1)).register(eq(tabSuggestionsOrchestrator));
        tabSuggestionsOrchestrator.destroy();
        verify(mDispatcher, times(1)).unregister(eq(tabSuggestionsOrchestrator));
    }

    @Test
    public void testTabFiltering() {
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(sTabs[0]).when(mTabModelFilter).getTabAt(eq(0));
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(mTabModelSelector, mDispatcher);
        List<TabSuggestion> suggestions = new LinkedList<>();
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions) {
                suggestions.addAll(tabSuggestions);
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(null, 0);
        Assert.assertEquals(0, suggestions.size());
    }
}
