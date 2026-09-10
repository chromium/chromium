// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.sms_fetcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link SmsFetcherMessageHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SmsFetcherMessageHandlerTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_normalCode() {
        assertEquals("123456", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("123456"));
        assertEquals("ABC-123", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("ABC-123"));
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_nullOrEmpty() {
        assertEquals("", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay(null));
        assertEquals("", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay(""));
        assertEquals("", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("   "));
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_stripsBiDiControls() {
        // U+202E (RLO - Right-to-Left Override)
        assertEquals(
                "951753", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("951753\u202E"));
        // Multiple BiDi controls: LRE, RLE, PDF, LRO, RLO, LRI, RLI, FSI, PDI
        String malicious = "951753\u202A\u202B\u202C\u202D\u202E\u2066\u2067\u2068\u2069";
        assertEquals("951753", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay(malicious));
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_stripsControlCharacters() {
        assertEquals(
                "123456",
                SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("12\u000034\u001F56"));
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_collapsesWhitespaceAndNBSP() {
        // Non-breaking spaces (U+00A0)
        assertEquals(
                "951753",
                SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("951753\u00A0\u00A0"));
        assertEquals(
                "951753 code",
                SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay("951753\u00A0\u00A0code"));
        // Flooding with 100 NBSP characters
        StringBuilder sb = new StringBuilder("951753");
        for (int i = 0; i < 100; i++) {
            sb.append("\u00A0");
        }
        assertEquals(
                "951753", SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay(sb.toString()));
    }

    @Test
    @SmallTest
    public void testSanitizeOneTimeCodeForDisplay_truncatesExcessiveLength() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 40; i++) {
            sb.append("a");
        }
        String result = SmsFetcherMessageHandler.sanitizeOneTimeCodeForDisplay(sb.toString());
        assertEquals(SmsFetcherMessageHandler.MAX_OTP_DISPLAY_LENGTH + 1, result.length());
        assertTrue(result.endsWith("…"));
        StringBuilder expected = new StringBuilder();
        for (int i = 0; i < SmsFetcherMessageHandler.MAX_OTP_DISPLAY_LENGTH; i++) {
            expected.append("a");
        }
        expected.append("…");
        assertEquals(expected.toString(), result);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)
    public void testGetNotificationTitle_defaultFormat() {
        String title =
                SmsFetcherMessageHandler.getNotificationTitle(
                        "951753\u202E", "example.com", null, "MacBook Pro");
        assertNotNull(title);
        // Ensure RLO is stripped
        assertFalse(title.contains("\u202E"));
        // Ensure OTP and client name are present
        assertTrue(title.contains("951753"));
        assertTrue(title.contains("MacBook Pro"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)
    public void testGetNotificationTitle_simpleStringFormat() {
        String title =
                SmsFetcherMessageHandler.getNotificationTitle(
                        "951753\u202E", "example.com", null, "MacBook Pro");
        assertNotNull(title);
        assertFalse(title.contains("\u202E"));
        assertTrue(title.contains("951753"));
        assertTrue(title.contains("example.com"));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)
    public void testGetNotificationText_defaultFormat() {
        String text =
                SmsFetcherMessageHandler.getNotificationText("example.com", null, "MacBook Pro");
        assertNotNull(text);
        assertTrue(text.contains("example.com"));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)
    public void testGetNotificationText_embeddedFrame() {
        String text =
                SmsFetcherMessageHandler.getNotificationText(
                        "top.com", "embedded.com", "MacBook Pro");
        assertNotNull(text);
        assertTrue(text.contains("top.com"));
        assertTrue(text.contains("embedded.com"));
    }
}
