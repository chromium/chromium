// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Tests for User Agent implementation. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class UserAgentTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public UserAgentTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
    }

    /**
     * Test for b/6404375. Verify that the UA string doesn't contain
     * two spaces before the Android build name.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    public void testNoExtraSpaceBeforeBuildName() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                // Spaces are replaced with underscores to avoid consecutive spaces compression.
                "<html>"
                        + "<body onload='document.title=navigator.userAgent.replace(/ /g, \"_\")'>"
                        + "</body>"
                        + "</html>",
                "text/html",
                false);
        final String ua = mActivityTestRule.getTitleOnUiThread(mAwContents);
        Matcher matcher = Pattern.compile("Android_[^;]+;_[^_]").matcher(ua);
        Assert.assertTrue(matcher.find());
    }
}
