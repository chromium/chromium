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
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedLoginRequestHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/** Tests for the AwContentsClient.onReceivedLoginRequest callback. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientAutoLoginTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    public AwContentsClientAutoLoginTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    private void autoLoginTestHelper(
            final String testName,
            final String xAutoLoginHeader,
            final String expectedRealm,
            final String expectedAccount,
            final String expectedArgs)
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
                /* testName= */ "testAutoLoginOnGoogleCom",
                /* xAutoLoginHeader= */ "realm=com.google&account=foo%40bar.com&args=random_string",
                /* expectedRealm= */ "com.google",
                /* expectedAccount= */ "foo@bar.com",
                /* expectedArgs= */ "random_string");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginWithNullAccount() throws Throwable {
        autoLoginTestHelper(
                /* testName= */ "testAutoLoginOnGoogleCom",
                /* xAutoLoginHeader= */ "realm=com.google&args=not.very.inventive",
                /* expectedRealm= */ "com.google",
                /* expectedAccount= */ null,
                /* expectedArgs= */ "not.very.inventive");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAutoLoginOnNonGoogle() throws Throwable {
        autoLoginTestHelper(
                /* testName= */ "testAutoLoginOnGoogleCom",
                /* xAutoLoginHeader= */ "realm=com.bar&account=foo%40bar.com&args=args",
                /* expectedRealm= */ "com.bar",
                /* expectedAccount= */ "foo@bar.com",
                /* expectedArgs= */ "args");
    }
}
