// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Test TabSuggestionsClientFetcher
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSuggestionsClientFetcherTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    TabContext mTabContext;

    @Mock
    private Callback<TabSuggestionsFetcherResults> mTabSuggestionsFetcherResultsCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    /**
     * Test when client fetcher has results
     */
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "close_tab_suggestions_stale_time_ms/86400000"})
    // 86400000 milliseconds = 1 day
    @Test
    public void
    testClientFetcher() {
        TabSuggestionsClientFetcher tabSuggestionsClientFetcher = new TabSuggestionsClientFetcher();
        // Ensures we call StaleTabSuggestionsProvider by ensuring stale tabs
        // are recommended to be closed.
        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(new TabContext.TabInfo(3, "mock_recent_title", "mock_recent_url",
                "mock_recent_original_url", "mock_recent_url",
                System.currentTimeMillis() - TimeUnit.MINUTES.toMillis(5)));
        tabInfos.add(new TabContext.TabInfo(3, "mock_stale_title", "mock_stale_url",
                "mock_stale_original_url", "mock_stale_referrer_url",
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2)));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();
        tabSuggestionsClientFetcher.fetch(mTabContext, mTabSuggestionsFetcherResultsCallback);
        ArgumentCaptor<TabSuggestionsFetcherResults> argument =
                ArgumentCaptor.forClass(TabSuggestionsFetcherResults.class);
        verify(mTabSuggestionsFetcherResultsCallback, times(1)).onResult(argument.capture());
        Assert.assertEquals(1, argument.getValue().tabSuggestions.size());
        TabSuggestion staleSuggestion = argument.getValue().tabSuggestions.get(0);
        Assert.assertEquals("mock_stale_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }

    /**
     * Test when results are null
     */
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "close_tab_suggestions_stale_time_ms/86400000"})
    // 86400000 milliseconds = 1 day
    @Test
    public void
    testNullResults() {
        TabSuggestionsClientFetcher tabSuggestionsClientFetcher = new TabSuggestionsClientFetcher();
        doReturn(null).when(mTabContext).getUngroupedTabs();
        tabSuggestionsClientFetcher.fetch(mTabContext, mTabSuggestionsFetcherResultsCallback);
        ArgumentCaptor<TabSuggestionsFetcherResults> argument =
                ArgumentCaptor.forClass(TabSuggestionsFetcherResults.class);
        verify(mTabSuggestionsFetcherResultsCallback, times(1)).onResult(argument.capture());
        Assert.assertEquals(0, argument.getValue().tabSuggestions.size());
    }

    /**
     * Test when results are empty
     */
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "close_tab_suggestions_stale_time_ms/86400000"})
    // 86400000 milliseconds = 1 day
    @Test
    public void
    testEmptyResults() {
        TabSuggestionsClientFetcher tabSuggestionsClientFetcher = new TabSuggestionsClientFetcher();
        doReturn(Collections.emptyList()).when(mTabContext).getUngroupedTabs();
        tabSuggestionsClientFetcher.fetch(mTabContext, mTabSuggestionsFetcherResultsCallback);
        ArgumentCaptor<TabSuggestionsFetcherResults> argument =
                ArgumentCaptor.forClass(TabSuggestionsFetcherResults.class);
        verify(mTabSuggestionsFetcherResultsCallback, times(1)).onResult(argument.capture());
        Assert.assertEquals(0, argument.getValue().tabSuggestions.size());
    }
}
