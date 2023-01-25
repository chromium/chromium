// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.any;
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
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

/**
 * Tests functionality of {@link TabSuggestionsOrchestrator}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowProcess.class)
public class TabSuggestionsOrchestratorTest {
    private static final int[] TAB_IDS = {0, 1, 2, 3, 4};
    private static final String GROUPING_PROVIDER = "groupingProvider";
    private static final String CLOSE_PROVIDER = "closeProvider";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    public Profile.Natives mMockProfileNatives;

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private TabModelFilter mTabModelFilter;

    @Mock
    private ActivityLifecycleDispatcher mDispatcher;

    private static Tab[] sTabs = new Tab[TAB_IDS.length];

    private static Tab mockTab(int id) {
        TabImpl tab = TabUiUnitTestUtils.prepareTab(id);
        WebContents webContents = mock(WebContents.class);
        doReturn(GURL.emptyGURL()).when(webContents).getVisibleUrl();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(GURL.emptyGURL()).when(tab).getOriginalUrl();
        doReturn(GURL.emptyGURL()).when(tab).getUrl();
        return tab;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ShadowProcess.reset();
        for (int i = 0; i < sTabs.length; i++) {
            sTabs[i] = mockTab(TAB_IDS[i]);
        }
        mocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(any(TabModelObserver.class));
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(Arrays.asList(sTabs[0])).when(mTabModelFilter).getRelatedTabList(0);
        doReturn(Arrays.asList(sTabs[1])).when(mTabModelFilter).getRelatedTabList(1);
        doReturn(Arrays.asList(sTabs[2])).when(mTabModelFilter).getRelatedTabList(2);
    }

    @Test
    public void verifyResultsPrefetched() {
        doReturn(TAB_IDS.length).when(mTabModelFilter).getCount();
        for (int idx = 0; idx < TAB_IDS.length; idx++) {
            doReturn(sTabs[idx]).when(mTabModelFilter).getTabAt(eq(idx));
        }
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(ContextUtils.getApplicationContext(),
                        mTabModelSelector, mDispatcher, new InMemorySharedPreferences());
        tabSuggestionsOrchestrator.setFetchersForTesting();
        List<TabSuggestion> suggestions = new LinkedList<>();
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
                    Callback<TabSuggestionFeedback> tabSuggestionFeedbackCallback) {
                suggestions.addAll(tabSuggestions);
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };
        tabSuggestionsOrchestrator.addObserver(tabSuggestionsObserver);
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND, false);
        Assert.assertEquals(1, suggestions.size());
        Assert.assertEquals(TAB_IDS.length, suggestions.get(0).getTabsInfo().size());
        for (int idx = 0; idx < TAB_IDS.length; idx++) {
            Assert.assertEquals(TAB_IDS[idx], suggestions.get(0).getTabsInfo().get(idx).id);
        }
    }

    @Test
    public void testRegisterUnregister() {
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(ContextUtils.getApplicationContext(),
                        mTabModelSelector, mDispatcher, new InMemorySharedPreferences());
        tabSuggestionsOrchestrator.setFetchersForTesting();
        verify(mDispatcher, times(1)).register(eq(tabSuggestionsOrchestrator));
        tabSuggestionsOrchestrator.onDestroy();
        verify(mDispatcher, times(1)).unregister(eq(tabSuggestionsOrchestrator));
    }

    @Test
    public void testTabFiltering() {
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(sTabs[0]).when(mTabModelFilter).getTabAt(eq(0));
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(ContextUtils.getApplicationContext(),
                        mTabModelSelector, mDispatcher, new InMemorySharedPreferences());
        tabSuggestionsOrchestrator.setFetchersForTesting();
        List<TabSuggestion> suggestions = new LinkedList<>();
        @SuppressWarnings("unused")
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
                    Callback<TabSuggestionFeedback> tabSuggestionFeedback) {
                suggestions.addAll(tabSuggestions);
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND, false);
        Assert.assertEquals(0, suggestions.size());
    }

    @Test
    public void testOrchestratorCallback() {
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(sTabs[0]).when(mTabModelFilter).getTabAt(eq(0));
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(ContextUtils.getApplicationContext(),
                        mTabModelSelector, mDispatcher, new InMemorySharedPreferences());
        tabSuggestionsOrchestrator.setFetchersForTesting();
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
                    Callback<TabSuggestionFeedback> tabSuggestionFeedback) {
                TabSuggestion tabSuggestion = new TabSuggestion(
                        Arrays.asList(TabContext.TabInfo.createFromTab(sTabs[0])), 0, "");
                tabSuggestionFeedback.onResult(new TabSuggestionFeedback(tabSuggestion,
                        TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED, Arrays.asList(1), 1));
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };

        tabSuggestionsOrchestrator.addObserver(tabSuggestionsObserver);
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND, false);
        Assert.assertNotNull(tabSuggestionsOrchestrator.mTabSuggestionFeedback);
        Assert.assertEquals(tabSuggestionsOrchestrator.mTabSuggestionFeedback.tabSuggestionResponse,
                TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED);
        Assert.assertEquals(tabSuggestionsOrchestrator.mTabSuggestionFeedback.totalTabCount, 1);
        Assert.assertEquals(
                tabSuggestionsOrchestrator.mTabSuggestionFeedback.selectedTabIds.size(), 1);
        Assert.assertEquals(
                (int) tabSuggestionsOrchestrator.mTabSuggestionFeedback.selectedTabIds.get(0), 1);
    }

    @Test
    public void testAggregationSorting() {
        TabSuggestion groupSuggestion =
                new TabSuggestion(Arrays.asList(TabContext.TabInfo.createFromTab(sTabs[0]),
                                          TabContext.TabInfo.createFromTab(sTabs[1])),
                        TabSuggestion.TabSuggestionAction.GROUP, GROUPING_PROVIDER);
        TabSuggestion closeSuggestion =
                new TabSuggestion(Arrays.asList(TabContext.TabInfo.createFromTab(sTabs[2]),
                                          TabContext.TabInfo.createFromTab(sTabs[3]),
                                          TabContext.TabInfo.createFromTab(sTabs[4])),
                        TabSuggestion.TabSuggestionAction.CLOSE, CLOSE_PROVIDER);
        List<TabSuggestion> sortedSuggestions = TabSuggestionsOrchestrator.aggregateResults(
                Arrays.asList(closeSuggestion, groupSuggestion));
        // Grouping suggestions should come first
        Assert.assertEquals(
                TabSuggestion.TabSuggestionAction.GROUP, sortedSuggestions.get(0).getAction());
        Assert.assertEquals(
                TabSuggestion.TabSuggestionAction.CLOSE, sortedSuggestions.get(1).getAction());
    }

    @Test
    public void testThrottlingNoRestriction() {
        // A second change to the tabmodel should result in suggestions
        // as the enforced time between prefetches is 0 (i.e. there is no
        // restriction)
        testThrottling(0, 1);
    }

    @Test
    public void testThrottlingFiveSecondsBetweenPrefetch() {
        // A second change to the tabmodel should not result in a second
        // set of suggestions because we force a break of 5 seconds between
        // calls
        testThrottling(5000, 0);
    }

    private void testThrottling(int minTimeBetweenPreFetches, int expectedSuggestions) {
        doReturn(TAB_IDS.length).when(mTabModelFilter).getCount();
        for (int idx = 0; idx < TAB_IDS.length; idx++) {
            doReturn(sTabs[idx]).when(mTabModelFilter).getTabAt(eq(idx));
        }
        TabSuggestionsOrchestrator tabSuggestionsOrchestrator =
                new TabSuggestionsOrchestrator(ContextUtils.getApplicationContext(),
                        mTabModelSelector, mDispatcher, new InMemorySharedPreferences());
        tabSuggestionsOrchestrator.setFetchersForTesting();
        tabSuggestionsOrchestrator.setMinTimeBetweenPreFetchesForTesting(minTimeBetweenPreFetches);
        final List<TabSuggestion> suggestions = new LinkedList<>();
        TabSuggestionsObserver tabSuggestionsObserver = new TabSuggestionsObserver() {
            @Override
            public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
                    Callback<TabSuggestionFeedback> tabSuggestionFeedbackCallback) {
                suggestions.addAll(tabSuggestions);
            }

            @Override
            public void onTabSuggestionInvalidated() {}
        };
        tabSuggestionsOrchestrator.setFetchersForTesting();
        tabSuggestionsOrchestrator.addObserver(tabSuggestionsObserver);
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND, false);
        Assert.assertEquals(1, suggestions.size());
        suggestions.clear();
        tabSuggestionsOrchestrator.mTabContextObserver.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND, false);
        Assert.assertEquals(expectedSuggestions, suggestions.size());
        tabSuggestionsOrchestrator.restoreMinTimeBetweenPrefetchesForTesting();
    }
}
