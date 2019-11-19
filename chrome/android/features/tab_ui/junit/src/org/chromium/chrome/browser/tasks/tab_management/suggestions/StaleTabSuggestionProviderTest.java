// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.doReturn;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests the provider which identifies Tabs which have not been used in a long time
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StaleTabSuggestionProviderTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    TabContext mTabContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    /**
     * Test identification of Tabs which have not been used in a long time (threshold set to 1 day)
     * and recommend to close them
     */
    @Test
    @Feature({"StaleTabSuggestionProvider"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "close_tab_suggestions_stale_time_ms/86400000"})
    // 86400000 milliseconds = 1 day
    public void
    testIdentifyStaleTabs() {
        StaleTabSuggestionProvider staleSuggestionsProvider = new StaleTabSuggestionProvider();
        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(new TabContext.TabInfo(3, "mock_recent_title", "mock_recent_url",
                "mock_recent_original_url", "mock_recent_url",
                System.currentTimeMillis() - TimeUnit.MINUTES.toMillis(5)));
        tabInfos.add(new TabContext.TabInfo(3, "mock_stale_title", "mock_stale_url",
                "mock_stale_original_url", "mock_stale_referrer_url",
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2)));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();
        List<TabSuggestion> staleSuggestions = staleSuggestionsProvider.suggest(mTabContext);
        Assert.assertTrue(staleSuggestions.size() == 1);
        TabSuggestion staleSuggestion = staleSuggestions.get(0);
        Assert.assertEquals("mock_stale_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }
}
