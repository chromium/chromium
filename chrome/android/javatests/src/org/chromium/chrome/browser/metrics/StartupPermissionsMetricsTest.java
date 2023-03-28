// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;

import android.Manifest;
import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/**
 * Tests for startup timing histograms.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class StartupPermissionsMetricsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock
    private AndroidPermissionDelegate mPermissionDelegate;

    private UmaSessionStats mUmaSessionStats;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUmaSessionStats = new UmaSessionStats(appContext); });
    }

    @Test
    @MediumTest
    public void testPermissionsGranted() throws Exception {
        doReturn(true)
                .when(mPermissionDelegate)
                .hasPermission(eq(Manifest.permission.RECORD_AUDIO));
        doReturn(true)
                .when(mPermissionDelegate)
                .canRequestPermission(eq(Manifest.permission.RECORD_AUDIO));

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "VoiceInteraction.AudioPermissionEvent.SessionStart",
                VoiceRecognitionHandler.AudioPermissionState.GRANTED);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mUmaSessionStats.startNewSession(null, mPermissionDelegate));
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPermissionsDeniedCanAsk() throws Exception {
        doReturn(false)
                .when(mPermissionDelegate)
                .hasPermission(eq(Manifest.permission.RECORD_AUDIO));
        doReturn(true)
                .when(mPermissionDelegate)
                .canRequestPermission(eq(Manifest.permission.RECORD_AUDIO));

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "VoiceInteraction.AudioPermissionEvent.SessionStart",
                VoiceRecognitionHandler.AudioPermissionState.DENIED_CAN_ASK_AGAIN);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mUmaSessionStats.startNewSession(null, mPermissionDelegate));
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPermissionsDeniedCannotAsk() throws Exception {
        doReturn(false)
                .when(mPermissionDelegate)
                .hasPermission(eq(Manifest.permission.RECORD_AUDIO));
        doReturn(false)
                .when(mPermissionDelegate)
                .canRequestPermission(eq(Manifest.permission.RECORD_AUDIO));

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "VoiceInteraction.AudioPermissionEvent.SessionStart",
                VoiceRecognitionHandler.AudioPermissionState.DENIED_CANNOT_ASK_AGAIN);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mUmaSessionStats.startNewSession(null, mPermissionDelegate));
        histogramWatcher.assertExpected();
    }
}
