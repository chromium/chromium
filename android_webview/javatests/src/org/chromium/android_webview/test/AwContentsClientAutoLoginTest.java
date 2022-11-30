// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedLoginRequestHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the AwContentsClient.onReceivedLoginRequest callback.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientAutoLoginTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private void autoLoginTestHelper(final String testName, final String xAutoLoginHeader,
            final String expectedRealm, final String expectedAccount, final String expectedArgs)
            throws Throwable {
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        final OnReceivedLoginRequestHelper loginRequestHelper =
                mContentsClient.getOnReceivedLoginRequestHelper();

        final String path = "/" + testName + ".html";
        final String html = testName;
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("x-auto-login", xAutoLoginHeader));

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse(path, html, headers);
            final int callCount = loginRequestHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(awContents, pageUrl);
            loginRequestHelper.waitForCallback(callCount);

            Assert.assertEquals(expectedRealm, loginRequestHelper.getRealm());
            Assert.assertEquals(expectedAccount, loginRequestHelper.getAccount());
            Assert.assertEquals(expectedArgs, loginRequestHelper.getArgs());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginOnGoogleCom() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.google&account=foo%40bar.com&args=random_string", /* xAutoLoginHeader */
                "com.google",  /* expectedRealm */
                "foo@bar.com",  /* expectedAccount */
                "random_string"  /* expectedArgs */);

    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginWithNullAccount() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.google&args=not.very.inventive", /* xAutoLoginHeader */
                "com.google",  /* expectedRealm */
                null,  /* expectedAccount */
                "not.very.inventive"  /* expectedArgs */);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginOnNonGoogle() throws Throwable {
        autoLoginTestHelper(
                "testAutoLoginOnGoogleCom",  /* testName */
                "realm=com.bar&account=foo%40bar.com&args=args", /* xAutoLoginHeader */
                "com.bar",  /* expectedRealm */
                "foo@bar.com",  /* expectedAccount */
                "args"  /* expectedArgs */);
    }
}
