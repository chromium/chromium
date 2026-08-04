// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.text.TextUtils;
import android.util.SparseArray;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateNewTabArguments;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * A profile + window tag-keyed service for collecting and reporting metrics from the
 * TabPersistentStore and TabStateStore.
 */
@NullMarked
public class TabStoreMetricsService {
    /** SharedPreferences key suffix for recording stored tab count. */
    public static final String TAB_COUNT_KEY_SUFFIX = "TabCount";

    /** SharedPreferences key suffix for recording stored tab group count. */
    public static final String GROUP_COUNT_KEY_SUFFIX = "GroupCount";

    /** SharedPreferences key suffix for recording stored pinned tab count. */
    public static final String PINNED_TAB_COUNT_KEY_SUFFIX = "PinnedTabCount";

    /** Histogram name prefix for recording total tab count delta. */
    public static final String HISTOGRAM_TAB_COUNT = "Tabs.TabStateStore.TabCount";

    /** Histogram name prefix for recording total tab group count delta. */
    public static final String HISTOGRAM_GROUP_COUNT = "Tabs.TabStateStore.GroupCount";

    /** Histogram name prefix for recording total pinned tab count delta. */
    public static final String HISTOGRAM_PINNED_TAB_COUNT = "Tabs.TabStateStore.PinnedTabCount";

    /** A bucket of metrics for a specific profile, window tag, and orchestrator type. */
    public static class MetricsBucket {
        public MetricsBucket(Profile profile, String windowTag, String orchestratorTag) {
            this.profile = profile;
            this.windowTag = windowTag;
            this.orchestratorTag = orchestratorTag;
        }

        public Profile profile;
        public String windowTag;
        public String orchestratorTag;

        /** Returns a tag combining profile, window tag, and orchestrator tag for key generation. */
        public String getTag() {
            String profileTag = profile.isOffTheRecord() ? "Incognito" : "Regular";
            return profileTag + "." + windowTag + "." + orchestratorTag;
        }

        @Override
        public boolean equals(Object o) {
            if (o == this) return true;
            if (!(o instanceof MetricsBucket other)) return false;
            return profile.equals(other.profile)
                    && windowTag.equals(other.windowTag)
                    && orchestratorTag.equals(other.orchestratorTag);
        }

        @Override
        public int hashCode() {
            return Objects.hash(profile, windowTag, orchestratorTag);
        }
    }

    private static final ProfileKeyedMap<TabStoreMetricsService> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.noRequiredCleanupAction());

    private final Map<MetricsBucket, WindowMetricsTracker> mTrackers = new HashMap<>();

    /** Private constructor to prevent direct instantiation. */
    private TabStoreMetricsService() {}

    /**
     * Retrieve the WindowMetricsTracker associated with a specific profile and window tag.
     *
     * @param profile The profile the service is associated with.
     * @param windowTag The tag identifying the window.
     */
    public static WindowMetricsTracker getForBucket(MetricsBucket bucket) {
        return sProfileMap
                .getForProfile(
                        bucket.profile.getOriginalProfile(), _ -> new TabStoreMetricsService())
                .getTracker(bucket);
    }

    /**
     * Retrieve the WindowMetricsTracker associated with a specific metrics bucket.
     *
     * @param bucket The metrics bucket to retrieve the tracker for.
     */
    private WindowMetricsTracker getTracker(MetricsBucket bucket) {
        WindowMetricsTracker tracker = mTrackers.get(bucket);
        if (tracker == null) {
            tracker = new WindowMetricsTracker(bucket);
            mTrackers.put(bucket, tracker);
        }
        return tracker;
    }

    /** Clears all saved tab store count metrics from SharedPreferences across all profiles. */
    public static void clearAllTabStoreCounts() {
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.TAB_STORE_METRICS);
    }

    /**
     * Clears saved tab store count metrics from SharedPreferences for a specific profile and window
     * tag.
     *
     * @param profile The profile whose count metrics should be cleared.
     * @param windowTag The window tag (e.g. window ID) whose count metrics should be cleared.
     */
    public static void clearTabStoreCountsForProfileAndWindow(Profile profile, String windowTag) {
        String profileTag = profile.isOffTheRecord() ? "Incognito" : "Regular";
        String infix = profileTag + "." + windowTag + ".";
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.TAB_STORE_METRICS, infix);
    }

    /** Tracks metrics for a single window instance. */
    public static class WindowMetricsTracker {
        /** Sentinel value when count metric preference is not set. */
        public static final int NO_COUNT_PREF = -1;

        private final Profile mProfile;
        private final String mWindowTag;
        private final String mOrchestratorTagSuffix;
        private final String mBucketTag;

        private WindowMetricsTracker(MetricsBucket bucket) {
            mProfile = bucket.profile;
            mWindowTag = bucket.windowTag;
            mOrchestratorTagSuffix = "." + bucket.orchestratorTag;
            mBucketTag = bucket.getTag();
        }

        private String getMetricKey(String metricName) {
            return ChromePreferenceKeys.TAB_STORE_METRICS.createKey(mBucketTag + "." + metricName);
        }

        /**
         * Helper method to store an integer count metric in SharedPreferences.
         *
         * @param metricName The metric name (e.g. TabCount) to append to the key.
         * @param count The count value to persist.
         */
        private void recordCountPref(String metricName, int count) {
            ChromeSharedPreferences.getInstance().writeInt(getMetricKey(metricName), count);
        }

        /**
         * Helper method to read an integer count metric from SharedPreferences.
         *
         * @param metricName The metric name (e.g. TabCount) to read from the key.
         * @return The persisted count value, or NO_COUNT_PREF if not present.
         */
        private int getCountPref(String metricName) {
            return ChromeSharedPreferences.getInstance()
                    .readInt(getMetricKey(metricName), NO_COUNT_PREF);
        }

        /**
         * Checks whether an integer count metric preference exists for this metric name.
         *
         * @param metricName The metric name (e.g. TabCount) to check.
         * @return True if a valid count pref exists, false otherwise.
         */
        public boolean hasCountPref(String metricName) {
            return getCountPref(metricName) != NO_COUNT_PREF;
        }

        /**
         * Persists the current total tab count to SharedPreferences.
         *
         * @param count The total number of tabs.
         */
        public void recordTabCount(int count) {
            recordCountPref(TAB_COUNT_KEY_SUFFIX, count);
        }

        /**
         * Persists the current total tab group count to SharedPreferences.
         *
         * @param count The total number of tab groups.
         */
        public void recordGroupCount(int count) {
            recordCountPref(GROUP_COUNT_KEY_SUFFIX, count);
        }

        /**
         * Persists the current total pinned tab count to SharedPreferences.
         *
         * @param count The total number of pinned tabs.
         */
        public void recordPinnedTabCount(int count) {
            recordCountPref(PINNED_TAB_COUNT_KEY_SUFFIX, count);
        }

        /**
         * Retrieves the persisted total tab count from SharedPreferences.
         *
         * @return The stored tab count.
         */
        public int getTabCount() {
            return getCountPref(TAB_COUNT_KEY_SUFFIX);
        }

        /**
         * Retrieves the persisted total tab group count from SharedPreferences.
         *
         * @return The stored tab group count.
         */
        public int getGroupCount() {
            return getCountPref(GROUP_COUNT_KEY_SUFFIX);
        }

        /**
         * Retrieves the persisted total pinned tab count from SharedPreferences.
         *
         * @return The stored pinned tab count.
         */
        public int getPinnedTabCount() {
            return getCountPref(PINNED_TAB_COUNT_KEY_SUFFIX);
        }

        /** Clears all saved tab store count metrics from SharedPreferences for this window. */
        public void clearTabStoreCounts() {
            TabStoreMetricsService.clearTabStoreCountsForProfileAndWindow(mProfile, mWindowTag);
        }

        private void recordCountDelta(String baseHistogram, int oldCount, int newCount) {
            int delta = newCount - oldCount;
            if (delta > 0) {
                RecordHistogram.recordCount1000Histogram(baseHistogram + "Delta.Positive", delta);
            } else if (delta < 0) {
                RecordHistogram.recordCount1000Histogram(baseHistogram + "Delta.Negative", -delta);
            }
        }

        /**
         * Counts pinned tabs and collects unique group IDs from tab creation data.
         *
         * @param dataList The list of tab creation data to analyze.
         * @param groupIds The set to collect unique tab group IDs into.
         * @return The count of pinned tabs in dataList.
         */
        private int countPinnedTabsAndCollectGroupIds(
                List<TabCreationData> dataList, Set<Token> groupIds) {
            int pinnedCount = 0;
            for (TabCreationData data : dataList) {
                if (data.isPinned) {
                    pinnedCount++;
                }
                if (data.tabGroupId != null) {
                    groupIds.add(data.tabGroupId);
                }
            }
            return pinnedCount;
        }

        /**
         * Reports fallback count and store discrepancies.
         *
         * @param authFrozenData The list of frozen tabs in the authoritative store.
         * @param authNewTabData The list of new tabs in the authoritative store.
         * @param shadowFrozenData The list of frozen tabs in the shadow store.
         * @param shadowNewTabData The list of new tabs in the shadow store.
         * @param shadowStoreCaughtUp Whether the shadow store has caught up.
         * @param regularFallbackTabs The map of tab IDs to URLs of regular fallback tabs created
         *     during restoration for the legacy store.
         */
        public void recordDiffMetrics(
                List<TabCreationData> authFrozenData,
                List<TabCreationData> authNewTabData,
                List<CreateFrozenTabArguments> shadowFrozenData,
                List<CreateNewTabArguments> shadowNewTabData,
                boolean shadowStoreCaughtUp,
                Map<@TabId Integer, String> regularFallbackTabs) {
            if (!shadowStoreCaughtUp) return;

            int authTabCount = authFrozenData.size() + authNewTabData.size();
            Set<Token> groupIds = new HashSet<>();
            int authPinnedCount = countPinnedTabsAndCollectGroupIds(authFrozenData, groupIds);
            authPinnedCount += countPinnedTabsAndCollectGroupIds(authNewTabData, groupIds);
            int authGroupCount = groupIds.size();

            boolean hasTabCount = hasCountPref(TAB_COUNT_KEY_SUFFIX);
            boolean hasGroupCount = hasCountPref(GROUP_COUNT_KEY_SUFFIX);
            boolean hasPinnedTabCount = hasCountPref(PINNED_TAB_COUNT_KEY_SUFFIX);

            int oldTabCount = getTabCount();
            int oldGroupCount = getGroupCount();
            int oldPinnedTabCount = getPinnedTabCount();

            recordTabCount(authTabCount);
            recordGroupCount(authGroupCount);
            recordPinnedTabCount(authPinnedCount);

            if (hasTabCount) {
                recordCountDelta(HISTOGRAM_TAB_COUNT, oldTabCount, authTabCount);
            }
            if (hasGroupCount) {
                recordCountDelta(HISTOGRAM_GROUP_COUNT, oldGroupCount, authGroupCount);
            }
            if (hasPinnedTabCount) {
                recordCountDelta(HISTOGRAM_PINNED_TAB_COUNT, oldPinnedTabCount, authPinnedCount);
            }

            int tabCountDelta =
                    (authNewTabData.size() + authFrozenData.size())
                            - (shadowFrozenData.size() + shadowNewTabData.size());

            if (tabCountDelta > 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher"
                                + mOrchestratorTagSuffix,
                        tabCountDelta);

            } else if (tabCountDelta < 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Tabs.TabStateStore.TabCountDelta.ShadowHigher" + mOrchestratorTagSuffix,
                        -tabCountDelta);
            } else {
                RecordHistogram.recordBooleanHistogram(
                        "Tabs.TabStateStore.TabCountDelta.Equal" + mOrchestratorTagSuffix, true);
            }

            int filteredFallbackTabCount =
                    calculateFilteredFallbackTabCount(
                            regularFallbackTabs, shadowFrozenData, shadowNewTabData);
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.RegularFallbackTabCount", filteredFallbackTabCount);

            SparseArray<TabCreationData> authoritativeDataMap =
                    new SparseArray<>(authFrozenData.size());
            for (TabCreationData data : authFrozenData) {
                authoritativeDataMap.put(data.id, data);
            }

            for (CreateFrozenTabArguments arguments : shadowFrozenData) {
                recordUrlMismatch(authoritativeDataMap.get(arguments.id), arguments);
            }
        }

        /**
         * Records URL mismatch metrics.
         *
         * @param authData The authoritative tab creation data.
         * @param shadowArgs The shadow tab creation arguments.
         */
        private void recordUrlMismatch(
                @Nullable TabCreationData authData, CreateFrozenTabArguments shadowArgs) {
            if (authData == null || shadowArgs.state.url == null) return;

            String authUrl = authData.url;
            String shadowUrl = shadowArgs.state.url.getSpec();
            if (TextUtils.equals(authUrl, shadowUrl)) return;

            long timeDelta = authData.timestampMillis - shadowArgs.state.timestampMillis;
            if (timeDelta > 0) {
                RecordHistogram.recordTimesHistogram(
                        "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer"
                                + mOrchestratorTagSuffix,
                        timeDelta);
            } else if (timeDelta < 0) {
                RecordHistogram.recordTimesHistogram(
                        "Tabs.TabStateStore.TimeDeltaOnMismatch.ShadowNewer"
                                + mOrchestratorTagSuffix,
                        -timeDelta);
            }
        }

        /**
         * Calculates the number of fallback tabs from the authoritative store that are not present
         * in the shadow store.
         *
         * @param regularFallbackTabs The map of tab IDs to URLs of regular fallback tabs.
         * @param shadowFrozenData The frozen tabs restored by the shadow store.
         * @param shadowNewTabData The new tabs created by the shadow store.
         * @return The count of fallback tabs not found in the shadow store.
         */
        private int calculateFilteredFallbackTabCount(
                Map<@TabId Integer, String> regularFallbackTabs,
                List<CreateFrozenTabArguments> shadowFrozenData,
                List<CreateNewTabArguments> shadowNewTabData) {
            Set<@TabId Integer> shadowTabIds = new HashSet<>();
            Set<String> shadowUrls = new HashSet<>();
            for (CreateFrozenTabArguments arg : shadowFrozenData) {
                shadowTabIds.add(arg.id);
                if (arg.state != null && arg.state.url != null) {
                    String spec = arg.state.url.getSpec();
                    if (spec.isEmpty()) {
                        spec = arg.state.url.getPossiblyInvalidSpec();
                    }
                    if (!spec.isEmpty()) {
                        shadowUrls.add(spec);
                    }
                }
            }
            for (CreateNewTabArguments arg : shadowNewTabData) {
                if (arg.loadUrlParams != null && arg.loadUrlParams.getUrl() != null) {
                    shadowUrls.add(arg.loadUrlParams.getUrl());
                }
            }

            int filteredFallbackTabCount = 0;
            for (Map.Entry<@TabId Integer, String> entry : regularFallbackTabs.entrySet()) {
                @TabId int tabId = entry.getKey();
                String url = entry.getValue();
                boolean presentInNewStore =
                        shadowTabIds.contains(tabId) || shadowUrls.contains(url);
                if (!presentInNewStore) {
                    filteredFallbackTabCount++;
                }
            }
            return filteredFallbackTabCount;
        }
    }
}
