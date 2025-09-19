// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.HashMap;
import java.util.Map;

/**
 * A metrics tracker for tab group suggestions. This class tracks user engagement with tab groups
 * created from suggestions, specifically for a single window.
 */
@NullMarked
public class SuggestionMetricsTracker implements Destroyable {
    /** The histogram prefix for the total number of tab switches within a session. */
    public static final String TOTAL_SWITCHES_HISTOGRAM_PREFIX =
            "GroupSuggestionsService.TabSwitches.Total.";

    /** The histogram prefix for the number of tab switches per group within a session. */
    public static final String PER_GROUP_SWITCHES_HISTOGRAM_PREFIX =
            "GroupSuggestionsService.TabSwitches.PerGroup.";

    /** The histogram prefix for the total time spent within a session. */
    public static final String TOTAL_TIME_SPENT_HISTOGRAM_PREFIX =
            "GroupSuggestionsService.TimeSpent.Total.";

    /** The histogram prefix for the time spent per group within a session. */
    public static final String PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX =
            "GroupSuggestionsService.TimeSpent.PerGroup.";

    /** A data class used to track metrics for a specific group type. */
    private static class GroupTypeMetricsCounts {
        /** The number of tab groups for this Group Type. */
        public int groupCount;

        /** The number of tab foreground switches for this Group Type. */
        public int foregroundSwitchCount;

        /** The total time spent with tabs in the foreground for this Group Type. */
        public long totalTimeSpentMs;
    }

    private final Map<Token, @GroupCreationSource Integer> mGroupIdToSuggestionType =
            new HashMap<>();
    private final Map<@GroupCreationSource Integer, GroupTypeMetricsCounts>
            mSuggestionTypeToCounts = new HashMap<>();
    private final Callback<@Nullable Tab> mOnCurrentTabChangedCallback = this::onChangeCurrentTab;
    private final ObservableSupplier<@Nullable Tab> mCurrentTabSupplier;

    private long mCurrentTabForegroundTime;
    private @Nullable Token mCurrentTabGroupId;

    /**
     * @param tabModelSelector The {@link TabModelSelector} to observe for tab changes.
     */
    public SuggestionMetricsTracker(TabModelSelector tabModelSelector) {
        resetSuggestionTypeToCounts();
        mCurrentTabSupplier =
                tabModelSelector.getModel(/* incognito= */ false).getCurrentTabSupplier();
        mCurrentTabSupplier.addObserver(mOnCurrentTabChangedCallback);
    }

    /** Resets the counters for a new session. */
    /*package*/ void reset() {
        mCurrentTabForegroundTime = 0;
        mCurrentTabGroupId = null;
        resetSuggestionTypeToCounts();
    }

    /** Records histograms for the current session. */
    /*package*/ void recordMetrics() {
        updateTimeSpent(SystemClock.elapsedRealtime());
        mSuggestionTypeToCounts.forEach(this::recordHistogramsForBin);
        mSuggestionTypeToCounts.clear();
        resetSuggestionTypeToCounts();
    }

    @Override
    public void destroy() {
        mCurrentTabSupplier.removeObserver(mOnCurrentTabChangedCallback);
    }

    private void resetSuggestionTypeToCounts() {
        mSuggestionTypeToCounts.put(
                GroupCreationSource.GTS_SUGGESTION, new GroupTypeMetricsCounts());
        mSuggestionTypeToCounts.put(
                GroupCreationSource.CPA_SUGGESTION, new GroupTypeMetricsCounts());
        mSuggestionTypeToCounts.put(GroupCreationSource.UNKNOWN, new GroupTypeMetricsCounts());
    }

    /**
     * Called when a tab group suggestion is accepted.
     *
     * @param suggestionType The type of suggestion that was accepted.
     * @param groupId The tab group ID of the suggested tab group.
     */
    /*package*/ void onSuggestionAccepted(
            @GroupCreationSource Integer suggestionType, Token groupId) {
        mGroupIdToSuggestionType.put(groupId, suggestionType);
        GroupTypeMetricsCounts counts = mSuggestionTypeToCounts.get(suggestionType);
        assert counts != null;
        counts.groupCount++;
    }

    private void onChangeCurrentTab(@Nullable Tab tab) {
        long currentTimeMs = SystemClock.elapsedRealtime();
        updateTimeSpent(currentTimeMs);

        if (tab == null) return;

        @GroupCreationSource int creationSource;
        Token groupId = tab.getTabGroupId();
        if (groupId == null) {
            return;
        } else if (mGroupIdToSuggestionType.containsKey(groupId)) {
            creationSource = mGroupIdToSuggestionType.get(groupId);
        } else {
            creationSource = GroupCreationSource.UNKNOWN;
            mGroupIdToSuggestionType.put(groupId, creationSource);

            GroupTypeMetricsCounts counts = mSuggestionTypeToCounts.get(creationSource);
            assert counts != null;

            counts.groupCount++;
        }

        GroupTypeMetricsCounts counts = mSuggestionTypeToCounts.get(creationSource);
        assert counts != null;

        counts.foregroundSwitchCount++;
    }

    private void updateTimeSpent(long currentTimeMs) {
        long previousTabForegroundTime = mCurrentTabForegroundTime;
        Token previousTabGroupId = mCurrentTabGroupId;

        mCurrentTabForegroundTime = currentTimeMs;
        Tab tab = mCurrentTabSupplier.get();
        mCurrentTabGroupId = tab != null ? tab.getTabGroupId() : null;

        if (previousTabGroupId == null || previousTabForegroundTime == 0) return;

        @GroupCreationSource
        int creationSource =
                mGroupIdToSuggestionType.getOrDefault(
                        previousTabGroupId, GroupCreationSource.UNKNOWN);
        GroupTypeMetricsCounts counts = mSuggestionTypeToCounts.get(creationSource);
        assert counts != null;

        counts.totalTimeSpentMs += currentTimeMs - previousTabForegroundTime;
    }

    private void recordHistogramsForBin(
            @GroupCreationSource Integer creationSource, GroupTypeMetricsCounts counts) {
        if (counts.groupCount == 0) return;
        String suffix = groupCreationSourceToSuffix(creationSource);
        RecordHistogram.recordCount100Histogram(
                TOTAL_SWITCHES_HISTOGRAM_PREFIX + suffix, counts.foregroundSwitchCount);

        RecordHistogram.recordCount100Histogram(
                PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + suffix,
                counts.foregroundSwitchCount / counts.groupCount);

        RecordHistogram.recordMediumTimesHistogram(
                TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + suffix, counts.totalTimeSpentMs);

        RecordHistogram.recordMediumTimesHistogram(
                PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + suffix,
                counts.totalTimeSpentMs / counts.groupCount);
    }

    private String groupCreationSourceToSuffix(@GroupCreationSource int source) {
        return switch (source) {
            case GroupCreationSource.GTS_SUGGESTION -> "GTS";
            case GroupCreationSource.CPA_SUGGESTION -> "CPA";
            case GroupCreationSource.UNKNOWN -> "Unknown";
            default -> throw new IllegalStateException();
        };
    }
}
