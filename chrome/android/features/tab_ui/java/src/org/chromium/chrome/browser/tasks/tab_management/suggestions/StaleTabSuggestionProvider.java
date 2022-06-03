// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Identifies stale Tabs using more advanced features (e.g. site engagement) and
 * more sophisticated data processing techniques (e.g. mean-variance normalization).
 */
public class StaleTabSuggestionProvider implements TabSuggestionProvider {
    // Since we are applying some statistical normalization, we need a minimum number of tabs.
    // That said, it also doesn't make sense to recommend closing stale tabs from a set of too few -
    // additional tabs give us context about what tab has gone stale.
    private static final int MIN_TABS_DEFAULT = 3;
    private static final double LAST_TIME_USED_DEFAULT_THRESHOLD = 0.5;
    private static final double SITE_ENGAGEMENT_DEFAULT_THRESHOLD = 100.0;
    private static final String TAG = "AdvStaleTabSuggest";

    @Override
    public List<TabSuggestion> suggest(TabContext tabContext) {
        if (tabContext == null || tabContext.getUngroupedTabs() == null
                || tabContext.getUngroupedTabs().size() < getMinNumTabs()
                || !isLastTimeUsedFeatureEnabled() && !isSiteEngagementFeatureEnabled()) {
            return null;
        }

        List<TabContext.TabInfo> staleTabs = new ArrayList<>();
        staleTabs.addAll(tabContext.getUngroupedTabs());

        if (isLastTimeUsedFeatureEnabled()) {
            String timeTransformString = getTimeTransformString();

            if (timeTransformString == null) {
                Log.e(TAG, "Time last used enabled but no transform string provided");
                return null;
            }

            @TransformMethod
            int transformMethod = fromTransformMethodString(timeTransformString);

            Map<TabContext.TabInfo, Double> values =
                    transformData(tabContext.getUngroupedTabs(), transformMethod);

            double timeThreshold = getLastTimeUsedFeatureThreshold();

            List<TabContext.TabInfo> staleTabsUsingTime = new ArrayList<>();
            for (Map.Entry<TabContext.TabInfo, Double> entry : values.entrySet()) {
                if (transformMethod == TransformMethod.MEAN_VARIANCE) {
                    if (entry.getValue() < timeThreshold) {
                        staleTabsUsingTime.add(entry.getKey());
                    }
                } else if (transformMethod == TransformMethod.DAY_NORMALIZATION) {
                    if (entry.getValue() > timeThreshold) {
                        staleTabsUsingTime.add(entry.getKey());
                    }
                }
            }
            staleTabs = staleTabsUsingTime;
        }

        if (isSiteEngagementFeatureEnabled()) {
            double siteEngagementThreshold = getSiteEngagementFeaturesThreshold();
            List<TabContext.TabInfo> staleTabsUsingSiteEngagement = new LinkedList<>();
            for (TabContext.TabInfo tab : staleTabs) {
                if (tab.getSiteEngagementScore() < siteEngagementThreshold) {
                    staleTabsUsingSiteEngagement.add(tab);
                }
            }
            staleTabs = staleTabsUsingSiteEngagement;
        }
        return Arrays.asList(new TabSuggestion(staleTabs, TabSuggestion.TabSuggestionAction.CLOSE,
                TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER));
    }

    private static Map<TabContext.TabInfo, Double> transformData(
            List<TabContext.TabInfo> tabs, @TransformMethod int transformMethod) {
        Map<TabContext.TabInfo, Double> res = new HashMap<>();
        switch (transformMethod) {
            case TransformMethod.MEAN_VARIANCE:
                double mean = calculateTimestampMean(tabs);
                for (TabContext.TabInfo tab : tabs) {
                    res.put(tab, tab.timestampMillis - mean);
                }
                double variance = calculateVariance(res.values());
                for (TabContext.TabInfo tab : tabs) {
                    res.put(tab, res.get(tab) / Math.sqrt(variance));
                }
                return res;
            case TransformMethod.DAY_NORMALIZATION:
                for (TabContext.TabInfo tab : tabs) {
                    res.put(tab,
                            ((double) (System.currentTimeMillis() - tab.timestampMillis))
                                    / TimeUnit.DAYS.toMillis(1));
                }
                return res;
        }
        // shouldn't reach here
        return res;
    }

    private static double calculateTimestampMean(List<TabContext.TabInfo> tabs) {
        if (tabs == null || tabs.isEmpty()) {
            return 0.0;
        }
        double sum = 0;
        for (TabContext.TabInfo tab : tabs) {
            sum += tab.timestampMillis;
        }
        return sum / tabs.size();
    }

    private static double calculateVariance(Collection<Double> values) {
        if (values == null || values.size() <= 1) {
            return 1.0;
        }
        double res = 0.0;
        for (Double value : values) {
            res += value * value;
        }
        return res;
    }

    public @interface TransformMethod {
        int DAY_NORMALIZATION = 0;
        int MEAN_VARIANCE = 1;
        int UNKNOWN = 2;
    }

    @TransformMethod
    int fromTransformMethodString(String str) {
        switch (str) {
            case "DAY_NORMALIZATION":
                return TransformMethod.DAY_NORMALIZATION;
            case "MEAN_VARIANCE":
                return TransformMethod.MEAN_VARIANCE;
        }
        // Shouldn't reach here
        return TransformMethod.UNKNOWN;
    }

    // Using non-static methods for static functions to enable mocking
    // since Mockito is not available

    /**
     * @return true if using the time last used feature is enabled for
     *         stale tab identification
     */
    protected boolean isLastTimeUsedFeatureEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                "close_tab_features_time_last_used_enabled", false);
    }

    /**
     * @return true if using the site engagement feature is enabled for
     *         stale tab identification
     */
    protected boolean isSiteEngagementFeatureEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                "close_tab_features_site_engagement_enabled", false);
    }

    /**
     * @return threshold for last time used feature for stale tab identification
     */
    protected double getLastTimeUsedFeatureThreshold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                "close_tab_features_time_last_used_threshold", LAST_TIME_USED_DEFAULT_THRESHOLD);
    }

    /**
     * @return threshold for site engagement feature for stale tab identification
     */
    protected double getSiteEngagementFeaturesThreshold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                "close_tab_features_site_engagement_threshold", SITE_ENGAGEMENT_DEFAULT_THRESHOLD);
    }

    /**
     * @return string for which transform to apply to time last used
     */
    protected String getTimeTransformString() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                "close_tab_features_time_last_used_transform");
    }

    /**
     * @return minimum number of tabs for doing stale tab identification.
     */
    protected int getMinNumTabs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS, "close_tab_suggestions_stale_time_ms",
                MIN_TABS_DEFAULT);
    }
}
