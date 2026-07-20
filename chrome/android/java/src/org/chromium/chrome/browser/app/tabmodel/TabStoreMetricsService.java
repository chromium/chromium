// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.text.TextUtils;
import android.util.SparseArray;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateNewTabArguments;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * A profile + window tag-keyed service for collecting and reporting metrics from the
 * TabPersistentStore and TabStateStore.
 */
@NullMarked
public class TabStoreMetricsService {
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
            tracker = new WindowMetricsTracker(bucket.orchestratorTag);
            mTrackers.put(bucket, tracker);
        }
        return tracker;
    }

    /** Tracks metrics for a single window instance. */
    public static class WindowMetricsTracker {
        private final String mSuffix;

        private WindowMetricsTracker(String orchestratorTag) {
            mSuffix = "." + orchestratorTag;
        }

        /**
         * Reports fallback count and store discrepancies.
         *
         * @param authFrozenData The list of frozen tabs in the authoritative store.
         * @param authNewTabData The list of new tabs in the authoritative store.
         * @param shadowFrozenData The list of frozen tabs in the shadow store.
         * @param shadowNewTabData The list of new tabs in the shadow store.
         * @param shadowStoreCaughtUp Whether the shadow store has caught up.
         * @param fallbackTabCount The number of fallback tabs created during restoration.
         */
        public void recordDiffMetrics(
                List<TabCreationData> authFrozenData,
                List<TabCreationData> authNewTabData,
                List<CreateFrozenTabArguments> shadowFrozenData,
                List<CreateNewTabArguments> shadowNewTabData,
                boolean shadowStoreCaughtUp,
                int fallbackTabCount) {
            if (!shadowStoreCaughtUp) return;

            int tabCountDelta =
                    (authNewTabData.size() + authFrozenData.size())
                            - (shadowFrozenData.size() + shadowNewTabData.size());

            if (tabCountDelta > 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher" + mSuffix,
                        tabCountDelta);

            } else if (tabCountDelta < 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Tabs.TabStateStore.TabCountDelta.ShadowHigher" + mSuffix, -tabCountDelta);
            } else {
                RecordHistogram.recordBooleanHistogram(
                        "Tabs.TabStateStore.TabCountDelta.Equal" + mSuffix, true);
            }

            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.RegularFallbackTabCount", fallbackTabCount);

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
                        "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer" + mSuffix,
                        timeDelta);
            } else if (timeDelta < 0) {
                RecordHistogram.recordTimesHistogram(
                        "Tabs.TabStateStore.TimeDeltaOnMismatch.ShadowNewer" + mSuffix, -timeDelta);
            }
        }
    }
}
