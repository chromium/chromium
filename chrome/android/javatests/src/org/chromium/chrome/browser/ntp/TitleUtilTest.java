// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.url.GURL;

/** Unit tests for TitleUtil. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TitleUtilTest {
    /** Tests for the getTitleForDisplay method. */
    @Test
    @SmallTest
    @Feature({"Ntp"})
    public void testGetTitleForDisplay() {
        // If the title is not null or empty it is simply returned.
        assertEquals(
                "title",
                TitleUtil.getTitleForDisplay("title", new GURL("https://example.com/path")));
        assertEquals("title", TitleUtil.getTitleForDisplay("title", GURL.emptyGURL()));
        assertEquals("title", TitleUtil.getTitleForDisplay("title", null));

        // If the url is null or empty the title is simply returned.
        assertNull(TitleUtil.getTitleForDisplay(null, null));
        assertEquals("", TitleUtil.getTitleForDisplay("", null));

        // If the title is null or empty but not the url, a shortened form of the url is returned.
        assertEquals(
                "example.com/foo/bar",
                TitleUtil.getTitleForDisplay(null, new GURL("https://example.com/foo/bar")));
        assertEquals(
                "example.com",
                TitleUtil.getTitleForDisplay(null, new GURL("https://example.com/")));
        assertEquals("foo/bar", TitleUtil.getTitleForDisplay(null, new GURL("file://foo/bar")));
        assertEquals(null, TitleUtil.getTitleForDisplay(null, new GURL("/")));
        assertEquals("", TitleUtil.getTitleForDisplay("", new GURL("/")));
    }
}
