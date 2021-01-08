// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
/**
 * Unit test for {@link ReadingListUtils}.
 */
public class ReadingListUtilsUnitTest {
    @Test
    @SmallTest
    public void testIsReadingListSupport() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(""));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported("chrome://flags"));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported("http://www.example.com"));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported("https://www.example.com"));
    }
}
