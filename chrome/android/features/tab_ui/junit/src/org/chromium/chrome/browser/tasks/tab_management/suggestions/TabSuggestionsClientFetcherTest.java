// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Test TabSuggestionsClientFetcher
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
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

    private TabContext.TabInfo getMockTab(int id, String title, String url, String originalUrl,
            String referrer, long timestamp, double siteEngagementScore) {
        TabContext.TabInfo tabInfo =
                spy(new TabContext.TabInfo(id, title, url, originalUrl, timestamp, ""));
        doReturn(siteEngagementScore).when(tabInfo).getSiteEngagementScore();
        return tabInfo;
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
        tabSuggestionsClientFetcher.setUseBaselineTabSuggestionsForTesting();
        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(3, "mock_stale_title", "mock_stale_url", "mock_stale_original_url",
                "mock_stale_referrer_url", System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2),
                0.0));
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
        tabSuggestionsClientFetcher.setUseBaselineTabSuggestionsForTesting();
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
        tabSuggestionsClientFetcher.setUseBaselineTabSuggestionsForTesting();
        doReturn(Collections.emptyList()).when(mTabContext).getUngroupedTabs();
        tabSuggestionsClientFetcher.fetch(mTabContext, mTabSuggestionsFetcherResultsCallback);
        ArgumentCaptor<TabSuggestionsFetcherResults> argument =
                ArgumentCaptor.forClass(TabSuggestionsFetcherResults.class);
        verify(mTabSuggestionsFetcherResultsCallback, times(1)).onResult(argument.capture());
        Assert.assertEquals(0, argument.getValue().tabSuggestions.size());
    }
}
