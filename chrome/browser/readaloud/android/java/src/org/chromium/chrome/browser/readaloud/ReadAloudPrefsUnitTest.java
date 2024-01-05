// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.components.prefs.PrefService;

import java.util.Map;

/** Unit tests for {@link ReadAloudPrefs}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudPrefsUnitTest {
    private MockPrefServiceHelper mMockPrefServiceHelper;
    private PrefService mPrefService;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock ReadAloudPrefs.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ReadAloudPrefsJni.TEST_HOOKS, mNativeMock);
        mMockPrefServiceHelper = new MockPrefServiceHelper();
        mPrefService = mMockPrefServiceHelper.getPrefService();
    }

    @Test
    public void testDefaultVoices() {
        Map<String, String> voices = ReadAloudPrefs.getVoices(mPrefService);
        assertTrue(voices.isEmpty());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testGetVoicesReadOnly() {
        // Modifying the returned map should throw an exception.
        ReadAloudPrefs.getVoices(mPrefService).put("en", "voice");
    }

    @Test
    public void testGetVoice() {
        MockPrefServiceHelper.setVoices(mNativeMock, Map.of("en", "voice"));

        Map<String, String> voices = ReadAloudPrefs.getVoices(mPrefService);
        assertEquals(1, voices.size());
        assertEquals("voice", voices.get("en"));
    }

    @Test
    public void testSetVoice() {
        ReadAloudPrefs.setVoice(mPrefService, "en", "voice");
        verify(mNativeMock).setVoice(eq(mPrefService), eq("en"), eq("voice"));
    }

    @Test
    public void testSetVoiceInvalid() {
        ReadAloudPrefs.setVoice(mPrefService, null, "voice");
        ReadAloudPrefs.setVoice(mPrefService, "en", null);
        ReadAloudPrefs.setVoice(mPrefService, "", "voice");
        ReadAloudPrefs.setVoice(mPrefService, "en", "");
        verify(mNativeMock, never()).setVoice(any(), any(), any());
    }

    @Test
    public void testDefaultSpeed() {
        assertEquals(1f, ReadAloudPrefs.getSpeed(mPrefService), /* delta= */ 0f);
    }

    @Test
    public void testGetSpeed() {
        mPrefService.setDouble("readaloud.speed", 2d);
        assertEquals(2f, ReadAloudPrefs.getSpeed(mPrefService), /* delta= */ 0f);
    }

    @Test
    public void testSetSpeed() {
        ReadAloudPrefs.setSpeed(mPrefService, 2f);
        verify(mPrefService).setDouble(eq("readaloud.speed"), eq(2d));
    }

    @Test
    public void testDefaultIsHighlightingEnabled() {
        assertEquals(true, ReadAloudPrefs.isHighlightingEnabled(mPrefService));
    }

    @Test
    public void testGetIsHighlightingEnabled() {
        mPrefService.setBoolean("readaloud.highlighting_enabled", false);
        assertEquals(false, ReadAloudPrefs.isHighlightingEnabled(mPrefService));
    }

    @Test
    public void testSetIsHighlightingEnabled() {
        ReadAloudPrefs.setHighlightingEnabled(mPrefService, false);
        verify(mPrefService).setBoolean(eq("readaloud.highlighting_enabled"), eq(false));
    }

    @Test
    public void testSpeedChanged_Metric() {
        ReadAloudPrefs.setSpeed(mPrefService, 1.0f);

        final String histogramName = "ReadAloud.SpeedChange";

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 3);
        ReadAloudPrefs.setSpeed(mPrefService, 1.2f);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 6);
        ReadAloudPrefs.setSpeed(mPrefService, 3.0f);
        histogram.assertExpected();
    }

    @Test
    public void testIsHighlightingEnabled_Metric() {
        ReadAloudPrefs.setHighlightingEnabled(mPrefService, false);

        final String histogramName = "ReadAloud.HighlightingEnabled";

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ReadAloudPrefs.setHighlightingEnabled(mPrefService, true);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        ReadAloudPrefs.setHighlightingEnabled(mPrefService, false);
        histogram.assertExpected();

        // test a duplicate isn't recorded
        histogram = HistogramWatcher.newBuilder().expectNoRecords(histogramName).build();
        ReadAloudPrefs.setHighlightingEnabled(mPrefService, false);
        histogram.assertExpected();
    }
}
