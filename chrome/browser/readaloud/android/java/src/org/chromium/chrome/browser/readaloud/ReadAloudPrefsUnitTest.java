// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.components.prefs.PrefService;

import java.util.Map;

/** Unit tests for {@link ReadAloudPrefs}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudPrefsUnitTest {
    private MockPrefServiceHelper mMockPrefServiceHelper;
    private PrefService mPrefService;

    @Captor private ArgumentCaptor<String> mPrefNameCaptor;
    @Captor private ArgumentCaptor<String> mPrefStringValueCaptor;
    @Captor private ArgumentCaptor<Boolean> mPrefBooleanValueCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMockPrefServiceHelper = new MockPrefServiceHelper();
        mPrefService = mMockPrefServiceHelper.getPrefService();
    }

    @After
    public void tearDown() {
        ReadAloudPrefs.resetForTesting();
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
        mPrefService.setString("readaloud.voices", "{\"en\":\"voice\"}");

        Map<String, String> voices = ReadAloudPrefs.getVoices(mPrefService);
        assertEquals(1, voices.size());
        assertEquals("voice", voices.get("en"));
    }

    @Test
    public void testSetVoice() throws JSONException {
        ReadAloudPrefs.setVoice(mPrefService, "en", "voice");
        verify(mPrefService).setString(eq("readaloud.voices"), mPrefStringValueCaptor.capture());

        var jsonObject = new JSONObject(mPrefStringValueCaptor.getValue());
        assertEquals("voice", jsonObject.get("en"));
    }

    @Test
    public void testSetVoiceInvalid() throws JSONException {
        ReadAloudPrefs.setVoice(mPrefService, null, "voice");
        ReadAloudPrefs.setVoice(mPrefService, "en", null);
        ReadAloudPrefs.setVoice(mPrefService, "", "voice");
        ReadAloudPrefs.setVoice(mPrefService, "en", "");
        verify(mPrefService, never()).setString(any(), any());
    }

    @Test
    public void testSetMultipleVoices() throws JSONException {
        ReadAloudPrefs.setVoice(mPrefService, "en", "voice");
        reset(mPrefService);
        ReadAloudPrefs.setVoice(mPrefService, "es", "voz");
        verify(mPrefService).setString(eq("readaloud.voices"), mPrefStringValueCaptor.capture());

        var jsonObject = new JSONObject(mPrefStringValueCaptor.getValue());
        assertEquals("voice", jsonObject.get("en"));
        assertEquals("voz", jsonObject.get("es"));
    }

    @Test
    public void testDefaultSpeed() {
        assertEquals(1f, ReadAloudPrefs.getSpeed(mPrefService), /* delta= */ 0f);
    }

    @Test
    public void testGetSpeed() {
        mPrefService.setString("readaloud.speed", "2.0");
        assertEquals(2f, ReadAloudPrefs.getSpeed(mPrefService), /* delta= */ 0f);
    }

    @Test
    public void testSetSpeed() {
        ReadAloudPrefs.setSpeed(mPrefService, 2f);
        verify(mPrefService).setString(eq("readaloud.speed"), eq("2.0"));
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
}
