// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tests for User Agent implementation.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class UserAgentTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mAwContents = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                              .getAwContents();
    }

    /**
     * Test for b/6404375. Verify that the UA string doesn't contain
     * two spaces before the Android build name.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoExtraSpaceBeforeBuildName() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        mActivityTestRule.loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                // Spaces are replaced with underscores to avoid consecutive spaces compression.
                "<html>"
                        + "<body onload='document.title=navigator.userAgent.replace(/ /g, \"_\")'>"
                        + "</body>"
                        + "</html>",
                "text/html", false);
        final String ua = mActivityTestRule.getTitleOnUiThread(mAwContents);
        Matcher matcher = Pattern.compile("Android_[^;]+;_[^_]").matcher(ua);
        Assert.assertTrue(matcher.find());
    }
}
