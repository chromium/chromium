// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link LinkToTextHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LinkToTextHelperTest {
    private static final String VISIBLE_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private Activity mActivity;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
    }

    @Test
    @SmallTest
    public void getUrlToShareTest() {
        String selector = "selector";
        String expectedUrlToShare = VISIBLE_URL + "#:~:text=selector";
        Assert.assertEquals(
                expectedUrlToShare, LinkToTextHelper.getUrlToShare(VISIBLE_URL, selector));
    }

    @Test
    @SmallTest
    public void getUrlToShareTest_URLWithFragment() {
        String selector = "selector";
        String expectedUrlToShare = VISIBLE_URL + "#:~:text=selector";
        Assert.assertEquals(
                expectedUrlToShare,
                LinkToTextHelper.getUrlToShare(VISIBLE_URL + "#elementid", selector));
    }

    @Test
    @SmallTest
    public void getUrlToShareTest_EmptySelector() {
        String selector = "";
        String expectedUrlToShare = VISIBLE_URL;
        Assert.assertEquals(
                expectedUrlToShare, LinkToTextHelper.getUrlToShare(VISIBLE_URL, selector));
    }

    @Test
    @SmallTest
    public void hasTextFragment() {
        GURL url = new GURL(VISIBLE_URL + "#:~:text=selector");
        Assert.assertEquals(true, LinkToTextHelper.hasTextFragment(url));
    }

    @Test
    @SmallTest
    public void hasTextFragment_URLWithNoTextSelector() {
        GURL url = new GURL(VISIBLE_URL);
        Assert.assertEquals(false, LinkToTextHelper.hasTextFragment(url));
    }
}
