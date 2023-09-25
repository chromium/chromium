// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.ThreadUtils.ThreadChecker;
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
    // FrameMetricsStore can only be accessed on the handler thread (from the
    // JankReportingScheduler.getOrCreateHandler() method). However construction occurs on a
    // separate thread so the ThreadChecker is instead constructed later.
    private ThreadChecker mThreadChecker;
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
    // Array of boolean values denoting whether a given frame is janky or not. Must always be the
    // same size as mTotalDurationsNs.
    private final ArrayList<Boolean> mIsJanky = new ArrayList<>();
    // Stores the timestamp (nanoseconds) of the most recent frame metric as a scenario started.
    // Zero if no FrameMetrics have been received.
    private final HashMap<Integer, Long> mScenarioPreviousFrameTimestampNs = new HashMap<>();

    // Convert an enum value to string to use as an UMA histogram name, changes to strings should be
    // reflected in android/histograms.xml and base/android/jank_
    public static String scenarioToString(@JankScenario int scenario) {
        switch (scenario) {
            case JankScenario.PERIODIC_REPORTING:
                return "Total";
            case JankScenario.OMNIBOX_FOCUS:
                return "OmniboxFocus";
            case JankScenario.NEW_TAB_PAGE:
                return "NewTabPage";
            case JankScenario.STARTUP:
                return "Startup";
            case JankScenario.TAB_SWITCHER:
                return "TabSwitcher";
            case JankScenario.OPEN_LINK_IN_NEW_TAB:
                return "OpenLinkInNewTab";
            case JankScenario.START_SURFACE_HOMEPAGE:
                return "StartSurfaceHomepage";
            case JankScenario.START_SURFACE_TAB_SWITCHER:
                return "StartSurfaceTabSwitcher";
            case JankScenario.FEED_SCROLLING:
                return "FeedScrolling";
            case JankScenario.WEBVIEW_SCROLLING:
                return "WebviewScrolling";
            default:
                throw new IllegalArgumentException("Invalid scenario value");
        }
    }

    /**
     * initialize is the first entry point that is on the HandlerThread, so set up our thread
     * checking.
     */
    void initialize() {
        mThreadChecker = new ThreadChecker();
    }

    /**
     * Records the total draw duration and jankiness for a single frame.
     */
    void addFrameMeasurement(long totalDurationNs, boolean isJanky, long frameStartVsyncTs) {
        mThreadChecker.assertOnValidThread();
        mTotalDurationsNs.add(totalDurationNs);
        mIsJanky.add(isJanky);
        mTimestampsNs.add(frameStartVsyncTs);
        mMaxTimestamp = frameStartVsyncTs;
    }

    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    void startTrackingScenario(@JankScenario int scenario) {
        try (TraceEvent e =
                        TraceEvent.scoped("startTrackingScenario: " + scenarioToString(scenario))) {
            mThreadChecker.assertOnValidThread();
            // Ignore multiple calls to startTrackingScenario without corresponding
            // stopTrackingScenario calls.
            if (mScenarioPreviousFrameTimestampNs.containsKey(scenario)) {
                return;
            }
            // Make a unique ID for each scenario for tracing.
            TraceEvent.startAsync(
                    "JankCUJ:" + scenarioToString(scenario), TRACE_EVENT_TRACK_ID + scenario);

            // Scenarios are tracked based on the latest stored timestamp to allow fast lookups
            // (find index of [timestamp] vs find first index that's >= [timestamp]). In case there
            // are no stored timestamps then we hardcode the scenario's starting timestamp to 0L,
            // this is handled as a special case in stopTrackingScenario by returning all stored
            // frames.
            Long startingTimestamp = 0L;
            if (!mTimestampsNs.isEmpty()) {
                startingTimestamp = mTimestampsNs.get(mTimestampsNs.size() - 1);
            }

            mScenarioPreviousFrameTimestampNs.put(scenario, startingTimestamp);
        }
    }

    boolean hasReceivedMetricsPast(long endScenarioTimeNs) {
        mThreadChecker.assertOnValidThread();
        return mMaxTimestamp > endScenarioTimeNs;
    }

    JankMetrics stopTrackingScenario(@JankScenario int scenario) {
        return stopTrackingScenario(scenario, -1);
    }

    // The string added is a static string.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    JankMetrics stopTrackingScenario(@JankScenario int scenario, long endScenarioTimeNs) {
        try (TraceEvent e =
                        TraceEvent.scoped("finishTrackingScenario: " + scenarioToString(scenario),
                                Long.toString(endScenarioTimeNs))) {
            mThreadChecker.assertOnValidThread();
            TraceEvent.finishAsync(
                    "JankCUJ:" + scenarioToString(scenario), TRACE_EVENT_TRACK_ID + scenario);
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

            int startingIndex;
            // Starting timestamp may be 0 if a scenario starts without any frames stored, in this
            // case return all frames.
            if (previousFrameTimestamp == 0) {
                startingIndex = 0;
            } else {
                startingIndex = mTimestampsNs.indexOf(previousFrameTimestamp);
                // The scenario starts with the frame after the tracking timestamp.
                startingIndex++;

                // If startingIndex is out of bounds then we haven't recorded any frames since
                // tracking started, return an empty FrameMetrics object.
                if (startingIndex >= mTimestampsNs.size()) {
                    return new JankMetrics();
                }
            }

            // Ending index is exclusive, so this is not out of bounds.
            int endingIndex = mTimestampsNs.size();
            if (endScenarioTimeNs > 0) {
                // binarySearch returns
                // index of the search key (non-negative value) or (-(insertion point) - 1).
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
                    convertArraysToJankMetrics(mTimestampsNs.subList(startingIndex, endingIndex),
                            mTotalDurationsNs.subList(startingIndex, endingIndex),
                            mIsJanky.subList(startingIndex, endingIndex));
            removeUnusedFrames();

            return jankMetrics;
        }
    }

    private void removeUnusedFrames() {
        if (mScenarioPreviousFrameTimestampNs.isEmpty()) {
            TraceEvent.instant("removeUnusedFrames", Long.toString(mTimestampsNs.size()));
            mTimestampsNs.clear();
            mTotalDurationsNs.clear();
            mIsJanky.clear();
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

        mTimestampsNs.subList(0, firstUsedIndex).clear();
        mTotalDurationsNs.subList(0, firstUsedIndex).clear();
        mIsJanky.subList(0, firstUsedIndex).clear();
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
            List<Long> longTimestampsNs, List<Long> longDurations, List<Boolean> booleanIsJanky) {
        long[] timestamps = new long[longTimestampsNs.size()];
        for (int i = 0; i < longTimestampsNs.size(); i++) {
            timestamps[i] = longTimestampsNs.get(i).longValue();
        }

        long[] durations = new long[longDurations.size()];
        for (int i = 0; i < longDurations.size(); i++) {
            durations[i] = longDurations.get(i).longValue();
        }

        boolean[] isJanky = new boolean[booleanIsJanky.size()];
        for (int i = 0; i < booleanIsJanky.size(); i++) {
            isJanky[i] = booleanIsJanky.get(i).booleanValue();
        }

        JankMetrics jankMetrics = new JankMetrics(timestamps, durations, isJanky);
        return jankMetrics;
    }
}
