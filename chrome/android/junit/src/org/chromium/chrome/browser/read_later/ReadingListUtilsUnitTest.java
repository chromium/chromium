// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ReadingListUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListUtilsUnitTest {
    @Test
    @SmallTest
    public void testIsReadingListSupport() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(GURL.emptyGURL()));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.HTTP_URL)));
    }
}
