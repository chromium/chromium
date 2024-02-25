// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link ReadingListUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListUtilsUnitTest {
    @Mock BookmarkModel mBookmarkModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doAnswer(
                        (invocation) -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());
    }

    @Test
    @SmallTest
    public void isReadingListSupported() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(GURL.emptyGURL()));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(JUnitTestGURLs.NTP_URL));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(JUnitTestGURLs.EXAMPLE_URL));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(JUnitTestGURLs.HTTP_URL));

        // empty url
        GURL testUrl = GURL.emptyGURL();
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(testUrl));

        // invalid url
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(JUnitTestGURLs.INVALID_URL));
    }
}
