// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/**
 * This class stores relevant metrics from FrameMetrics between the calls to UMA reporting methods.
 */
public class FrameMetricsStore {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    // An arbitrary value from which to create a trace event async track. The only risk if this
    // clashes with another track is trace events will show up on both potentially looking weird in
    // the tracing UI. No other issue will occur.
    private static final long TRACE_EVENT_TRACK_ID = 84186319646187624L;
    // Android FrameMetrics promises in order frame metrics so this is just the latest timestamp.
    private long mMaxTimestamp = -1;
    // Array of timestamps stored in nanoseconds, they represent the moment when each frame
    // began (VSYNC_TIMESTAMP), must always be the same size as mTotalDurationsNs.
    private final ArrayList<Long> mTimestampsNs = new ArrayList<>();
    // Array of total durations stored in nanoseconds, they represent how long each frame took to
    // draw.
    private final ArrayList<Long> mTotalDurationsNs = new ArrayList<>();
    // Array of integers denoting number of vsyncs we missed for given frame. 0 missed vsyncs mean
    // no jank, while >0 missed vsyncs mean the frame was janky. Must always be the same size as
    // mTotalDurationsNs.
    private final ArrayList<Integer> mNumMissedVsyncs = new ArrayList<>();
    // Stores the timestamp (nanoseconds) of the most recent frame metric as a scenario started.
    // Zero if no FrameMetrics have been received.
    private final HashMap<JankScenario, Long> mScenarioPreviousFrameTimestampNs = new HashMap<>();
    private final HashMap<JankScenario, Long> mPendingStartTimestampNs = new HashMap<>();

    public FrameMetricsStore() {
        // Add 0 to mTimestampNS array. This simplifies handling edge case when starting a scenario
        // and we don't have any frame metrics stored. Adding 0 also makes sure the array stays in
        // sorted order since the actual metrics received will have larger vsync start timestamps.
        mTimestampsNs.add(0L);
        // Add arbitrary values to related arrays as well since we always want them to be of same
        // size.
        mTotalDurationsNs.add(0L);
        mNumMissedVsyncs.add(0);
    }

    // Convert an enum value to string to use as an UMA histogram name, changes to strings should be
    // reflected in android/histograms.xml and base/android/jank_
    public static String scenarioToString(@JankScenario.Type int scenario) {
        switch (scenario) {
            case JankScenario.Type.PERIODIC_REPORTING:
                return "Total";
            case JankScenario.Type.OMNIBOX_FOCUS:
                return "OmniboxFocus";
            case JankScenario.Type.NEW_TAB_PAGE:
                return "NewTabPage";
            case JankScenario.Type.STARTUP:
                return "Startup";
            case JankScenario.Type.TAB_SWITCHER:
                return "TabSwitcher";
            case JankScenario.Type.OPEN_LINK_IN_NEW_TAB:
                return "OpenLinkInNewTab";
            case JankScenario.Type.START_SURFACE_HOMEPAGE:
                return "StartSurfaceHomepage";
            case JankScenario.Type.START_SURFACE_TAB_SWITCHER:
                return "StartSurfaceTabSwitcher";
            case JankScenario.Type.FEED_SCROLLING:
                return "FeedScrolling";
            case JankScenario.Type.WEBVIEW_SCROLLING:
                return "WebviewScrolling";
            case JankScenario.Type.COMBINED_WEBVIEW_SCROLLING:
                return "CombinedWebviewScrolling";
            default:
                throw new IllegalArgumentException("Invalid scenario value");
        }
    }

    /**
     * initialize is the first entry point that is on the HandlerThread, so set up our thread
     * checking.
     */
    void initialize() {
        // FrameMetricsStore can only be accessed on the handler thread (from the
        // JankReportingScheduler.getOrCreateHandler() method). However construction occurs on a
        // separate thread so the ThreadChecker is instead constructed later.
        mThreadChecker.resetThreadId();
    }

    /** Records the total draw duration and jankiness for a single frame. */
    void addFrameMeasurement(long totalDurationNs, int numMissedVsyncs, long frameStartVsyncTs) {
        mThreadChecker.assertOnValidThread();
        mTotalDurationsNs.add(totalDurationNs);
        mNumMissedVsyncs.add(numMissedVsyncs);
        mTimestampsNs.add(frameStartVsyncTs);
        mMaxTimestamp = frameStartVsyncTs;
    }

    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    void startTrackingScenario(JankScenario scenario) {
        try (TraceEvent e =
                TraceEvent.scoped("startTrackingScenario: " + scenarioToString(scenario.type()))) {
            mThreadChecker.assertOnValidThread();
            // Ignore multiple calls to startTrackingScenario without corresponding
            // stopTrackingScenario calls.
            if (mScenarioPreviousFrameTimestampNs.containsKey(scenario)) {
                mPendingStartTimestampNs.put(
                        scenario, TimeUtils.uptimeMillis() * TimeUtils.NANOSECONDS_PER_MILLISECOND);
                return;
            }
            // Make a unique ID for each scenario for tracing.
            TraceEvent.startAsync(
                    "JankCUJ:" + scenarioToString(scenario.type()),
                    TRACE_EVENT_TRACK_ID + scenario.type());
            // Scenarios are tracked based on the latest stored timestamp to allow fast lookups
            // (find index of [timestamp] vs find first index that's >= [timestamp]).
            Long startingTimestamp = mTimestampsNs.get(mTimestampsNs.size() - 1);
            mScenarioPreviousFrameTimestampNs.put(scenario, startingTimestamp);
        }
    }

    boolean hasReceivedMetricsPast(long endScenarioTimeNs) {
        mThreadChecker.assertOnValidThread();
        return mMaxTimestamp > endScenarioTimeNs;
    }

    JankMetrics stopTrackingScenario(JankScenario scenario) {
        return stopTrackingScenario(scenario, -1);
    }

    // The string added is a static string.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    JankMetrics stopTrackingScenario(JankScenario scenario, long endScenarioTimeNs) {
        try (TraceEvent e =
                TraceEvent.scoped(
                        "finishTrackingScenario: " + scenarioToString(scenario.type()),
                        Long.toString(endScenarioTimeNs))) {
            mThreadChecker.assertOnValidThread();
            TraceEvent.finishAsync(
                    "JankCUJ:" + scenarioToString(scenario.type()),
                    TRACE_EVENT_TRACK_ID + scenario.type());
            // Get the timestamp of the latest frame before startTrackingScenario was called. This
            // can be null if tracking never started for scenario, or 0L if tracking started when no
            // frames were stored.
            Long previousFrameTimestamp = mScenarioPreviousFrameTimestampNs.remove(scenario);

            // If stopTrackingScenario is called without a corresponding startTrackingScenario then
            // return an empty FrameMetrics object.
            if (previousFrameTimestamp == null) {
                removeUnusedFrames();
                return new JankMetrics();
            }

            int startingIndex = mTimestampsNs.indexOf(previousFrameTimestamp);
            // The scenario starts with the frame after the tracking timestamp.
            startingIndex++;

            // If startingIndex is out of bounds then we haven't recorded any frames since
            // tracking started, return an empty FrameMetrics object.
            if (startingIndex >= mTimestampsNs.size()) {
                return new JankMetrics();
            }

            // Ending index is exclusive, so this is not out of bounds.
            int endingIndex = mTimestampsNs.size();
            if (endScenarioTimeNs > 0) {
                // binarySearch returns index of the search key (non-negative value) or (-(insertion
                // point) - 1).
                // The insertion point is defined as the index of the first element greater than the
                // key, or a.length if all elements in the array are less than the specified key.
                endingIndex = Collections.binarySearch(mTimestampsNs, endScenarioTimeNs);
                if (endingIndex < 0) {
                    endingIndex = -1 * (endingIndex + 1);
                } else {
                    endingIndex = Math.min(endingIndex + 1, mTimestampsNs.size());
                }
                if (endingIndex <= startingIndex) {
                    // Something went wrong reset
                    TraceEvent.instant("FrameMetricsStore invalid endScenarioTimeNs");
                    endingIndex = mTimestampsNs.size();
                }
            }

            JankMetrics jankMetrics =
                    convertArraysToJankMetrics(
                            mTimestampsNs.subList(startingIndex, endingIndex),
                            mTotalDurationsNs.subList(startingIndex, endingIndex),
                            mNumMissedVsyncs.subList(startingIndex, endingIndex));
            removeUnusedFrames();

            Long pendingStartTimestampNs = mPendingStartTimestampNs.remove(scenario);
            if (pendingStartTimestampNs != null && pendingStartTimestampNs > endScenarioTimeNs) {
                startTrackingScenario(scenario);
            }
            return jankMetrics;
        }
    }

    private void removeUnusedFrames() {
        if (mScenarioPreviousFrameTimestampNs.isEmpty()) {
            TraceEvent.instant("removeUnusedFrames", Long.toString(mTimestampsNs.size()));
            mTimestampsNs.subList(1, mTimestampsNs.size()).clear();
            mTotalDurationsNs.subList(1, mTotalDurationsNs.size()).clear();
            mNumMissedVsyncs.subList(1, mNumMissedVsyncs.size()).clear();
            return;
        }

        long firstUsedTimestamp = findFirstUsedTimestamp();
        // If the earliest timestamp tracked is 0 then that scenario contains every frame
        // stored, so we shouldn't delete anything.
        if (firstUsedTimestamp == 0L) {
            return;
        }

        int firstUsedIndex = mTimestampsNs.indexOf(firstUsedTimestamp);
        if (firstUsedIndex == -1) {
            if (BuildConfig.ENABLE_ASSERTS) {
                throw new IllegalStateException("Timestamp for tracked scenario not found");
            }
            // This shouldn't happen.
            return;
        }
        TraceEvent.instant("removeUnusedFrames", Long.toString(firstUsedIndex));

        mTimestampsNs.subList(1, firstUsedIndex).clear();
        mTotalDurationsNs.subList(1, firstUsedIndex).clear();
        mNumMissedVsyncs.subList(1, firstUsedIndex).clear();
    }

    private long findFirstUsedTimestamp() {
        long firstTimestamp = Long.MAX_VALUE;
        for (long timestamp : mScenarioPreviousFrameTimestampNs.values()) {
            if (timestamp < firstTimestamp) {
                firstTimestamp = timestamp;
            }
        }

        return firstTimestamp;
    }

    private JankMetrics convertArraysToJankMetrics(
            List<Long> longTimestampsNs,
            List<Long> longDurations,
            List<Integer> intNumMissedVsyncs) {
        long[] timestamps = new long[longTimestampsNs.size()];
        for (int i = 0; i < longTimestampsNs.size(); i++) {
            timestamps[i] = longTimestampsNs.get(i).longValue();
        }

        long[] durations = new long[longDurations.size()];
        for (int i = 0; i < longDurations.size(); i++) {
            durations[i] = longDurations.get(i).longValue();
        }

        int[] numMissedVsyncs = new int[intNumMissedVsyncs.size()];
        for (int i = 0; i < intNumMissedVsyncs.size(); i++) {
            numMissedVsyncs[i] = intNumMissedVsyncs.get(i).intValue();
        }

        JankMetrics jankMetrics = new JankMetrics(timestamps, durations, numMissedVsyncs);
        return jankMetrics;
    }
}
