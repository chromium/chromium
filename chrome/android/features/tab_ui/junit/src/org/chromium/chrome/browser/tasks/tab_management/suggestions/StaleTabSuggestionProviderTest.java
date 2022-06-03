// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.CALLS_REAL_METHODS;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests the provider which identifies Tabs which have not been used in a long time
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StaleTabSuggestionProviderTest {
    // Hard code current time in milliseconds to ensure stable test
    private static final long CURRENT_TIME_MILLIS = 1573866756832L;
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    TabContext mTabContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private TabContext.TabInfo getMockTab(int id, String title, String url, String originalUrl,
            String referrer, long timestamp, double siteEngagementScore) {
        TabContext.TabInfo tabInfo =
                spy(new TabContext.TabInfo(id, title, url, originalUrl, referrer, timestamp, ""));
        doReturn(siteEngagementScore).when(tabInfo).getSiteEngagementScore();
        return tabInfo;
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProviderMeanVariance"})
    public void testTimeLastUsedFeatureMeanVariance() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(true);
        when(staleTabSuggestionProvider.getLastTimeUsedFeatureThreshold()).thenReturn(0.0);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(3);
        when(staleTabSuggestionProvider.getTimeTransformString()).thenReturn("MEAN_VARIANCE");

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_recent_title_1", "mock_recent_url_1",
                "mock_recent_original_url_1", "mock_recent_referrer_1",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 1.0));
        tabInfos.add(getMockTab(2, "mock_recent_title_2", "mock_recent_url_2",
                "mock_recent_original_url_2", "mock_recent_referrer_2",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(20), 1.0));
        tabInfos.add(getMockTab(3, "mock_recent_title_3", "mock_recent_url_3",
                "mock_recent_original_url_3", "mock_recent_referrer_3",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(36), 1.0));
        tabInfos.add(getMockTab(4, "mock_stale_title", "mock_stale_url_1",
                "mock_stale_original_url", "mock_stale_referrer_url",
                CURRENT_TIME_MILLIS - TimeUnit.DAYS.toMillis(4), 1.0));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();

        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertEquals(1, staleSuggestions.size());
        TabSuggestion staleSuggestion = staleSuggestions.get(0);
        Assert.assertEquals("mock_stale_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProvider"})
    public void testTimeLastUsedFeatureDayNormalization() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(true);
        when(staleTabSuggestionProvider.getLastTimeUsedFeatureThreshold()).thenReturn(3.0);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(3);
        when(staleTabSuggestionProvider.getTimeTransformString()).thenReturn("DAY_NORMALIZATION");

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_recent_title_1", "mock_recent_url_1",
                "mock_recent_original_url_1", "mock_recent_referrer_1",
                System.currentTimeMillis() - TimeUnit.MINUTES.toMillis(5), 1.0));
        tabInfos.add(getMockTab(2, "mock_recent_title_2", "mock_recent_url_2",
                "mock_recent_original_url_2", "mock_recent_referrer_2",
                System.currentTimeMillis() - TimeUnit.MINUTES.toMillis(20), 1.0));
        tabInfos.add(getMockTab(3, "mock_recent_title_3", "mock_recent_url_3",
                "mock_recent_original_url_3", "mock_recent_referrer_3",
                System.currentTimeMillis() - TimeUnit.MINUTES.toMillis(36), 1.0));
        tabInfos.add(getMockTab(4, "mock_stale_title", "mock_stale_url_1",
                "mock_stale_original_url", "mock_stale_referrer_url",
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(4), 1.0));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();

        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertEquals(1, staleSuggestions.size());
        TabSuggestion staleSuggestion = staleSuggestions.get(0);
        Assert.assertEquals("mock_stale_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProvider"})
    public void testSiteEngagementFeature() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(true);
        when(staleTabSuggestionProvider.getSiteEngagementFeaturesThreshold()).thenReturn(2.0);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(3);

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_very_engaged_title_1", "mock_very_engaged_url_1",
                "mock_very_engaged_url_1", "mock_very_engaged_referrer_1",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 20.0));
        tabInfos.add(getMockTab(2, "mock_very_engaged_title_2", "mock_very_engaged_url_2",
                "mock_very_engaged_url_2", "mock_very_engaged_referrer_2",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 25.0));
        tabInfos.add(getMockTab(3, "mock_medium_engaged_title", "mock_medium_engaged_url",
                "mock_very_medium_url", "mock_very_engaged_referrer",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 10.0));
        tabInfos.add(getMockTab(4, "mock_low_engaged_title", "mock_low_engaged_url",
                "mock_low_medium_url", "mock_low_engaged_referrer",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 1.0));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();

        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertEquals(1, staleSuggestions.size());
        TabSuggestion staleSuggestion = staleSuggestions.get(0);
        Assert.assertEquals("mock_low_engaged_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProvider"})
    public void testTimeLastUsedFeatureAndSiteEngagement() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(true);
        when(staleTabSuggestionProvider.getLastTimeUsedFeatureThreshold()).thenReturn(0.0);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(true);
        when(staleTabSuggestionProvider.getSiteEngagementFeaturesThreshold()).thenReturn(2.0);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(3);
        when(staleTabSuggestionProvider.getTimeTransformString()).thenReturn("MEAN_VARIANCE");

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_recent_title_1", "mock_recent_url_1",
                "mock_recent_original_url_1", "mock_recent_referrer_1",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 1.0));
        tabInfos.add(getMockTab(2, "mock_recent_title_2", "mock_recent_url_2",
                "mock_recent_original_url_2", "mock_recent_referrer_2",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(20), 1.0));
        tabInfos.add(getMockTab(3, "mock_recent_title_3", "mock_recent_url_3",
                "mock_recent_original_url_3", "mock_recent_referrer_3",
                CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(36), 1.0));
        tabInfos.add(getMockTab(4, "mock_stale_title", "mock_stale_url", "mock_stale_original_url",
                "mock_stale_referrer_url", CURRENT_TIME_MILLIS - TimeUnit.DAYS.toMillis(4), 1.0));
        tabInfos.add(getMockTab(5, "mock_stale_title_highly_engaged",
                "mock_stale_url_highly_engaged", "mock_stale_original_url_highly_engaged",
                "mock_stale_referrer_url_highly_engaged",
                CURRENT_TIME_MILLIS - TimeUnit.DAYS.toMillis(4), 20.0));

        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();

        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertEquals(1, staleSuggestions.size());
        TabSuggestion staleSuggestion = staleSuggestions.get(0);
        Assert.assertEquals("mock_stale_title", staleSuggestion.getTabsInfo().get(0).title);
        Assert.assertEquals(TabSuggestion.TabSuggestionAction.CLOSE, staleSuggestion.getAction());
        Assert.assertEquals(TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER,
                staleSuggestion.getProviderName());
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProvider"})
    public void testNoFeaturesActive() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(1);

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_title", "mock_url", "mock_original_url",
                "mock_recent_referrer", CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 1.0));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();
        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertNull(staleSuggestions);
    }

    @Test
    @Feature({"AdvancedStaleTabSuggestionProvider"})
    public void testNotEnoughTabs() {
        StaleTabSuggestionProvider staleTabSuggestionProvider =
                mock(StaleTabSuggestionProvider.class, CALLS_REAL_METHODS);
        when(staleTabSuggestionProvider.isLastTimeUsedFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.isSiteEngagementFeatureEnabled()).thenReturn(false);
        when(staleTabSuggestionProvider.getMinNumTabs()).thenReturn(3);

        List<TabContext.TabInfo> tabInfos = new ArrayList<>();
        tabInfos.add(getMockTab(1, "mock_title", "mock_url", "mock_original_url",
                "mock_recent_referrer_1", CURRENT_TIME_MILLIS - TimeUnit.MINUTES.toMillis(5), 1.0));
        doReturn(tabInfos).when(mTabContext).getUngroupedTabs();
        List<TabSuggestion> staleSuggestions = staleTabSuggestionProvider.suggest(mTabContext);
        Assert.assertNull(staleSuggestions);
    }
}
