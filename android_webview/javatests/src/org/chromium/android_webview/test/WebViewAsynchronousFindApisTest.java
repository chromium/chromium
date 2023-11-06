// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;

/** Tests the asynchronous find-in-page APIs in WebView. */
@RunWith(AwJUnit4ClassRunner.class)
public class WebViewAsynchronousFindApisTest {
    @Rule public WebViewFindApisTestRule mActivityTestRule = new WebViewFindApisTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllFinds() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDouble() throws Throwable {
        mActivityTestRule.findAllAsyncOnUiThread("wood");
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("chuck"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDoubleNext() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(2, mActivityTestRule.findNextOnUiThread(true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllDoesNotFind() throws Throwable {
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread("foo"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyPage() throws Throwable {
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread("foo"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyString() throws Throwable {
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread(""));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextForward() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));

        for (int i = 2; i <= 4; i++) {
            Assert.assertEquals(i - 1, mActivityTestRule.findNextOnUiThread(true));
        }
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextBackward() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));

        for (int i = 4; i >= 1; i--) {
            Assert.assertEquals(i - 1, mActivityTestRule.findNextOnUiThread(false));
        }
        Assert.assertEquals(3, mActivityTestRule.findNextOnUiThread(false));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextBig() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));

        Assert.assertEquals(1, mActivityTestRule.findNextOnUiThread(true));
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(false));
        Assert.assertEquals(3, mActivityTestRule.findNextOnUiThread(false));
        for (int i = 1; i <= 4; i++) {
            Assert.assertEquals(i - 1, mActivityTestRule.findNextOnUiThread(true));
        }
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindAllEmptyNext() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(1, mActivityTestRule.findNextOnUiThread(true));
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread(""));
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(true));
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread(""));
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(1, mActivityTestRule.findNextOnUiThread(true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testClearMatches() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        mActivityTestRule.clearMatchesOnUiThread();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testClearFindNext() throws Throwable {
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        mActivityTestRule.clearMatchesOnUiThread();
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(1, mActivityTestRule.findNextOnUiThread(true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindEmptyNext() throws Throwable {
        Assert.assertEquals(0, mActivityTestRule.findAllAsyncOnUiThread(""));
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(true));
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "FindInPage"})
    public void testFindNextFirst() throws Throwable {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mActivityTestRule.contents().findNext(true));
        Assert.assertEquals(4, mActivityTestRule.findAllAsyncOnUiThread("wood"));
        Assert.assertEquals(1, mActivityTestRule.findNextOnUiThread(true));
        Assert.assertEquals(0, mActivityTestRule.findNextOnUiThread(false));
        Assert.assertEquals(3, mActivityTestRule.findNextOnUiThread(false));
    }
}
