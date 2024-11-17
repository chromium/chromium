// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class CctPasswordSavingMetricsRecorderBridgeTest {
    private static final long INCREMENT_MS = 10;

    private UnownedUserDataHost mUnownedUserDataHost;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mUnownedUserDataHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
    }

    @After
    public void tearDown() {
        ShadowSystemClock.reset();
    }

    @Test
    public void testonPotentialSaveFormSubmittedAttachesToWindowAndDestroyDetaches() {
        CctPasswordSavingMetricsRecorderBridge recorderBridge =
                new CctPasswordSavingMetricsRecorderBridge(mWindowAndroid);
        recorderBridge.onPotentialSaveFormSubmitted();
        assertNotNull(
                CctPasswordSavingMetricsRecorderBridge.KEY.retrieveDataFromHost(
                        mWindowAndroid.getUnownedUserDataHost()));
        recorderBridge.destroy();
        assertNull(
                CctPasswordSavingMetricsRecorderBridge.KEY.retrieveDataFromHost(
                        mWindowAndroid.getUnownedUserDataHost()));
    }

    @Test
    public void testRecordingMetricAtRedirect() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        CctPasswordSavingMetricsRecorderBridge
                                .SUBMISSION_TO_REDIRECT_TIME_HISTOGRAM,
                        (int) INCREMENT_MS);

        CctPasswordSavingMetricsRecorderBridge recorderBridge =
                new CctPasswordSavingMetricsRecorderBridge(mWindowAndroid);
        recorderBridge.onPotentialSaveFormSubmitted();
        ShadowSystemClock.advanceBy(INCREMENT_MS, TimeUnit.MILLISECONDS);
        recorderBridge.onExternalNavigation();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordsMetricsAtActivityStopIfRedirect() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CctPasswordSavingMetricsRecorderBridge
                                        .SUBMISSION_TO_REDIRECT_TIME_HISTOGRAM,
                                (int) INCREMENT_MS)
                        .expectIntRecord(
                                CctPasswordSavingMetricsRecorderBridge
                                        .SUBMISSION_TO_ACTIVITY_STOP_TIME_HISTOGRAM,
                                (int) (2 * INCREMENT_MS))
                        .expectIntRecord(
                                CctPasswordSavingMetricsRecorderBridge
                                        .REDIRECT_TO_ACTIVITY_STOP_TIME_HISTOGRAM,
                                (int) INCREMENT_MS)
                        .build();
        CctPasswordSavingMetricsRecorderBridge recorderBridge =
                new CctPasswordSavingMetricsRecorderBridge(mWindowAndroid);
        recorderBridge.onPotentialSaveFormSubmitted();

        ShadowSystemClock.advanceBy(INCREMENT_MS, TimeUnit.MILLISECONDS);
        recorderBridge.onExternalNavigation();

        ShadowSystemClock.advanceBy(INCREMENT_MS, TimeUnit.MILLISECONDS);
        recorderBridge.onActivityStopped();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDoesntRecordActivityStopMetricsIfNoRedirect() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                CctPasswordSavingMetricsRecorderBridge
                                        .SUBMISSION_TO_REDIRECT_TIME_HISTOGRAM)
                        .expectNoRecords(
                                CctPasswordSavingMetricsRecorderBridge
                                        .SUBMISSION_TO_ACTIVITY_STOP_TIME_HISTOGRAM)
                        .expectNoRecords(
                                CctPasswordSavingMetricsRecorderBridge
                                        .REDIRECT_TO_ACTIVITY_STOP_TIME_HISTOGRAM)
                        .build();
        CctPasswordSavingMetricsRecorderBridge recorderBridge =
                new CctPasswordSavingMetricsRecorderBridge(mWindowAndroid);
        recorderBridge.onPotentialSaveFormSubmitted();
        recorderBridge.onActivityStopped();
        histogramWatcher.assertExpected();
    }
}
