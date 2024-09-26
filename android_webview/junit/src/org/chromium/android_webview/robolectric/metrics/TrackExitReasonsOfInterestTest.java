// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.metrics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AppState;
import org.chromium.android_webview.metrics.TrackExitReasonsOfInterest;
import org.chromium.android_webview.metrics.TrackExitReasonsOfInterest.ExitReasonData;
import org.chromium.base.Callback;
import org.chromium.base.PathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem.ExitReason;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** Junit tests for TrackExitReasonsOfInterest. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, manifest = Config.NONE)
public class TrackExitReasonsOfInterestTest {
    private static final String TAG = "ExitReasonsTest";
    private MockAwContentsLifecycleNotifier mTestSupplier = new MockAwContentsLifecycleNotifier();

    @Before
    public void setUp() {
        // Needed in case the data directory is not initialized for testing
        PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
        TrackExitReasonsOfInterest.setStateSupplier(mTestSupplier::getAppState);
        mTestSupplier.mState = AppState.UNKNOWN;
    }

    public static class MockAwContentsLifecycleNotifier {
        public @AppState int mState;

        public MockAwContentsLifecycleNotifier() {
            mState = AppState.UNKNOWN;
        }

        public MockAwContentsLifecycleNotifier(@AppState int appState) {
            mState = appState;
        }

        @AppState
        int getAppState() {
            return mState;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReadWriteExitReasonUtils() {
        int previousPid = 1;
        long timeAtLastRecording = 5L;
        TrackExitReasonsOfInterest.writeLastExitInfo(
                new ExitReasonData(previousPid, timeAtLastRecording, AppState.UNKNOWN));
        assertTrue(
                "last-exit-info file should exist after writing to it",
                TrackExitReasonsOfInterest.getLastExitInfoFile().exists());
        ExitReasonData data = TrackExitReasonsOfInterest.readLastExitInfo();
        assertEquals(
                "Last exit info PID should be stored in last-exit-info file",
                previousPid,
                data.mExitInfoPid);
        assertEquals(
                "Last exit info timestamp should be stored in last-exit-info file",
                timeAtLastRecording,
                data.mTimestampAtLastRecordingInMillis);
        assertEquals(
                "Last exit info timestamp should be stored in last-exit-info file",
                AppState.UNKNOWN,
                data.mState);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExitReasonMarkedAsInvalidWhenFileDoesNotExist() {
        ActivityManager mockedActivityManager =
                getMockedActivityManager(
                        /* pid= */ 1, ApplicationExitInfo.REASON_ANR, /* isEmpty= */ true);
        ProcessExitReasonFromSystem.setActivityManagerForTest(mockedActivityManager);
        assertEquals(
                "Last exit info data should be created after first run",
                -1,
                TrackExitReasonsOfInterest.run());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWriteAppStateOnlyModifiesAppStateInFile() throws TimeoutException {
        int previousPid = 1;
        long timeAtLastRecording = 5L;
        TrackExitReasonsOfInterest.setPidForTest(previousPid);
        TrackExitReasonsOfInterest.setCurrtimeForTest(timeAtLastRecording);
        TrackExitReasonsOfInterest.writeLastExitInfo(
                new ExitReasonData(previousPid, timeAtLastRecording, AppState.DESTROYED));
        ExitReasonData data = TrackExitReasonsOfInterest.readLastExitInfo();
        // pids are updated
        assertTrue(data.mExitInfoPid == previousPid);
        // timestamps are updated
        assertTrue(data.mTimestampAtLastRecordingInMillis == timeAtLastRecording);
        assertTrue(data.mState == AppState.DESTROYED);

        final CallbackHelper writeFinished = new CallbackHelper();
        final Callback<Boolean> callback =
                new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean result) {
                        writeFinished.notifyCalled();
                    }
                };
        int calls;
        for (@AppState int appState = AppState.UNKNOWN; appState < AppState.DESTROYED; appState++) {
            mTestSupplier.mState = appState;
            calls = writeFinished.getCallCount();
            TrackExitReasonsOfInterest.writeLastWebViewState(callback);
            writeFinished.waitForCallback(calls);
            data = TrackExitReasonsOfInterest.readLastExitInfo();
            // pids are updated
            assertTrue(data.mExitInfoPid == previousPid);
            // timestamps are updated
            assertTrue(data.mTimestampAtLastRecordingInMillis == timeAtLastRecording);
            assertEquals(data.mState, appState);
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testWriteAppStateTaskRunnerOnlyUpdatesWithNewState() throws TimeoutException {
        int previousPid = 1;
        long timeAtLastRecording = 5L;
        final CallbackHelper writeFinished = new CallbackHelper();
        final Callback<Boolean> callback =
                new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean result) {
                        writeFinished.notifyCalled();
                    }
                };
        int calls;
        TrackExitReasonsOfInterest.setPidForTest(previousPid);
        TrackExitReasonsOfInterest.setCurrtimeForTest(timeAtLastRecording);
        TrackExitReasonsOfInterest.writeLastExitInfo(
                new ExitReasonData(previousPid, timeAtLastRecording, AppState.DESTROYED));
        ExitReasonData data = TrackExitReasonsOfInterest.readLastExitInfo();
        // pids are updated
        assertTrue(data.mExitInfoPid == previousPid);
        // timestamps are updated
        assertTrue(data.mTimestampAtLastRecordingInMillis == timeAtLastRecording);
        assertTrue(data.mState == AppState.DESTROYED);

        mTestSupplier.mState = AppState.FOREGROUND;
        calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.writeLastWebViewState(callback);
        writeFinished.waitForCallback(calls);

        data = TrackExitReasonsOfInterest.readLastExitInfo();
        // pids are updated
        assertTrue(data.mExitInfoPid == previousPid);
        // timestamps are updated
        assertTrue(data.mTimestampAtLastRecordingInMillis == timeAtLastRecording);
        assertEquals(data.mState, mTestSupplier.getAppState());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExitReasonMarkedAsInvalidWhenExitReasonsFileDoesNotHavePreviousPid() {
        int previousPid = 0;
        ActivityManager mockedActivityManager =
                getMockedActivityManager(
                        previousPid, ApplicationExitInfo.REASON_ANR, /* isEmpty= */ false);
        ProcessExitReasonFromSystem.setActivityManagerForTest(mockedActivityManager);
        TrackExitReasonsOfInterest.writeLastExitInfo(
                new ExitReasonData(previousPid + 1, 5L, AppState.UNKNOWN));
        assertEquals(
                "Last exit info data should be created after first run",
                -1,
                TrackExitReasonsOfInterest.run());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExpectedExitReasons() {
        int previousPid = 1;
        long timeAtLastRecording = 5L;
        long systemTimeForTest = 10L;
        TrackExitReasonsOfInterest.setSystemTimeForTest(systemTimeForTest);
        for (int mockedSystemExitReason = 0;
                mockedSystemExitReason < ExitReason.NUM_ENTRIES;
                mockedSystemExitReason++) {
            ActivityManager mockedActivityManager =
                    getMockedActivityManager(
                            previousPid, mockedSystemExitReason, /* isEmpty= */ false);
            ProcessExitReasonFromSystem.setActivityManagerForTest(mockedActivityManager);
            TrackExitReasonsOfInterest.writeLastExitInfo(
                    new ExitReasonData(previousPid, timeAtLastRecording, AppState.UNKNOWN));
            Integer exitReasonData =
                    ProcessExitReasonFromSystem.convertApplicationExitInfoToExitReason(
                            TrackExitReasonsOfInterest.run());
            Integer exitReason =
                    ProcessExitReasonFromSystem.convertApplicationExitInfoToExitReason(
                            mockedSystemExitReason);
            assertEquals(
                    "Last exit info data should be created after first run",
                    exitReason,
                    exitReasonData);
            assertTrue(
                    "Exit reason should be within the expected range of exit reasons: "
                            + exitReason
                            + " "
                            + mockedSystemExitReason,
                    exitReasonData != null);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistogramsAreUpdatedWithValidLastExitInfoSet() {
        int previousPid = 1;
        long timeAtLastRecording = 5L;
        long systemTimeForTest = 10L;
        TrackExitReasonsOfInterest.setSystemTimeForTest(systemTimeForTest);
        // Mock current PID
        TrackExitReasonsOfInterest.setPidForTest(previousPid + 1);
        for (int mockedSystemExitReason = 0;
                mockedSystemExitReason < ExitReason.NUM_ENTRIES;
                mockedSystemExitReason++) {
            for (@AppState int state = 0; state <= AppState.DESTROYED; state++) {
                ActivityManager mockedActivityManager =
                        getMockedActivityManager(
                                previousPid, mockedSystemExitReason, /* isEmpty= */ false);
                ProcessExitReasonFromSystem.setActivityManagerForTest(mockedActivityManager);
                TrackExitReasonsOfInterest.writeLastExitInfo(
                        new ExitReasonData(previousPid, timeAtLastRecording, state));

                var histogramWatcher =
                        HistogramWatcher.newBuilder()
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_COUNTS
                                                + "."
                                                + TrackExitReasonsOfInterest.sUmaSuffixMap.get(
                                                        state),
                                        ProcessExitReasonFromSystem
                                                .convertApplicationExitInfoToExitReason(
                                                        mockedSystemExitReason))
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_COUNTS,
                                        ProcessExitReasonFromSystem
                                                .convertApplicationExitInfoToExitReason(
                                                        mockedSystemExitReason))
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_DELTA,
                                        (int) (systemTimeForTest - timeAtLastRecording))
                                .build();
                TrackExitReasonsOfInterest.run();
                histogramWatcher.assertExpected();

                ExitReasonData data = TrackExitReasonsOfInterest.readLastExitInfo();
                // pids are updated
                assertTrue(
                        "Pid in last-exit-info should be updated with current PID"
                                + data.mExitInfoPid
                                + " "
                                + previousPid,
                        data.mExitInfoPid != previousPid);
                // timestamps are updated
                assertTrue(
                        "Timestamp in last-exit-info should be updated with latest system time",
                        data.mTimestampAtLastRecordingInMillis != timeAtLastRecording);
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExitReasonDataBuilderProvidesExpectedValues() {
        int testPid = 10;
        long testTimestampAtLastRecordingInMillis = 12345;
        for (@AppState int testState = AppState.UNKNOWN;
                testState <= AppState.DESTROYED;
                testState++) {
            ExitReasonData data =
                    new ExitReasonData(testPid, testTimestampAtLastRecordingInMillis, testState);
            assertEquals("Last exit info data PIDs should match", testPid, data.mExitInfoPid);
            assertEquals("Last exit info data's AppState should match", testState, data.mState);
            assertEquals(
                    "Last exit info data's timestamps should match",
                    testTimestampAtLastRecordingInMillis,
                    data.mTimestampAtLastRecordingInMillis);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLastExitInfoIsFilledAfterExecution() {
        int previousPid = 50;
        TrackExitReasonsOfInterest.setPidForTest(previousPid);
        ActivityManager mockedActivityManager =
                getMockedActivityManager(
                        previousPid, ApplicationExitInfo.REASON_ANR, /* isEmpty= */ false);
        ProcessExitReasonFromSystem.setActivityManagerForTest(mockedActivityManager);
        TrackExitReasonsOfInterest.run();

        ExitReasonData data = TrackExitReasonsOfInterest.readLastExitInfo();
        assertTrue(
                "Last exit info pid should be created after first run " + data.mExitInfoPid,
                data != null && data.mExitInfoPid == previousPid);
        assertTrue(
                "Last exit info timestamp should be created after first run "
                        + data.mTimestampAtLastRecordingInMillis,
                data.mTimestampAtLastRecordingInMillis != 0L);
    }

    // Helper methods below

    private ActivityManager getMockedActivityManager(
            int pid, int systemExitReason, boolean isEmpty) {
        ActivityManager mockedActivityManager = Mockito.mock(ActivityManager.class);
        // Note: "aei" is just short hand for ApplicationExitInfo
        ApplicationExitInfo aei = Mockito.mock(ApplicationExitInfo.class);
        when(aei.getReason()).thenReturn(systemExitReason);
        when(aei.getPid()).thenReturn(pid);

        Stream<ApplicationExitInfo> s = Stream.of(aei);
        List<ApplicationExitInfo> aeiList;
        if (isEmpty) {
            aeiList = Arrays.asList();
        } else {
            aeiList = s.collect(Collectors.toList());
        }
        when(mockedActivityManager.getHistoricalProcessExitReasons(null, pid, 1))
                .thenReturn(aeiList);

        return mockedActivityManager;
    }
}
