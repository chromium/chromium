// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Unit tests for TitleUtil.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TitleUtilTest {
    /**
     * Tests for the getTitleForDisplay method.
     */
    @Test
    @Feature({"Ntp"})
    public void testGetTitleForDisplay() {
        // If the title is not null or empty it is simply returned.
        assertEquals("title", TitleUtil.getTitleForDisplay("title", "https://example.com/path"));
        assertEquals("title", TitleUtil.getTitleForDisplay("title", ""));
        assertEquals("title", TitleUtil.getTitleForDisplay("title", null));

        // If the url is null or empty the title is simply returned.
        assertNull(TitleUtil.getTitleForDisplay(null, null));
        assertEquals("", TitleUtil.getTitleForDisplay("", null));

        // If the title is null or empty but not the url, a shortened form of the url is returned.
        assertEquals("example.com/foo/bar",
                TitleUtil.getTitleForDisplay(null, "https://example.com/foo/bar"));
        assertEquals("example.com", TitleUtil.getTitleForDisplay(null, "https://example.com/"));
        assertEquals("foo/bar", TitleUtil.getTitleForDisplay(null, "foo/bar"));
        assertEquals("", TitleUtil.getTitleForDisplay(null, "/"));
    }
}
