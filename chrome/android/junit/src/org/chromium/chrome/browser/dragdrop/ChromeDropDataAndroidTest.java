// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ChromeDropDataAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ChromeDropDataAndroidTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;

    @Test
    public void testBuildTabClipDataText() {
        when(mTab.getId()).thenReturn(1);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        ChromeDropDataAndroid data = new ChromeDropDataAndroid.Builder().withTab(mTab).build();
        assertEquals(
                "Clip data text is not as expected.",
                "TabId=1\n" + JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                data.buildTabClipDataText());
    }

    @Test
    public void testBuildTabClipDataTextWithNullTab() {
        ChromeDropDataAndroid data = new ChromeDropDataAndroid.Builder().build();
        assertNull("Clip data text is not as expected.", data.buildTabClipDataText());
    }

    @Test
    public void testExtractTabId() {
        String validText = "TabId=1\n" + JUnitTestGURLs.EXAMPLE_URL.getSpec();
        assertEquals(
                "Extracted TabId is not as expected.",
                1,
                ChromeDropDataAndroid.extractTabId(validText));
    }

    @Test
    public void testExtractTabIdWithInvalidText() {
        String textWithNoDelimiter = "text";
        assertInvalidText(textWithNoDelimiter);

        String textWithDelimiterButNoId = "TabId=\nsome text";
        assertInvalidText(textWithDelimiterButNoId);

        String textWithInvalidId = "TabId=abc\nsome text";
        assertInvalidText(textWithInvalidId);

        String emptyString = "";
        assertInvalidText(emptyString);

        String nullString = null;
        assertInvalidText(nullString);
    }

    private void assertInvalidText(String input) {
        assertEquals(
                "Extracted TabId is not as expected.",
                Tab.INVALID_TAB_ID,
                ChromeDropDataAndroid.extractTabId(input));
    }
}
