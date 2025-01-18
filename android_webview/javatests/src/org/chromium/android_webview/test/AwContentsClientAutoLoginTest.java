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


       // Configure a web server to serve a test page
        final String path = "/" + testName + ".html"; // Dynamically set path
        final String html = testName; // Simplified HTML content
        List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(Pair.create("x-auto-login", xAutoLoginHeader));

        TestWebServer webServer = TestWebServer.start(); // Start server
        try {
            final String pageUrl = webServer.setResponse(path, html, headers); // Serve content
            final int callCount = loginRequestHelper.getCallCount(); // Capture initial count
            mActivityTestRule.loadUrlAsync(awContents, pageUrl); // Load URL asynchronously
            loginRequestHelper.waitForCallback(callCount); // Wait for callback

            // Assert the received values match expectations
            Assert.assertEquals(expectedRealm, loginRequestHelper.getRealm());
            Assert.assertEquals(expectedAccount, loginRequestHelper.getAccount());
            Assert.assertEquals(expectedArgs, loginRequestHelper.getArgs());
        } finally {
            // Ensure the server is shut down properly
            webServer.shutdown(); // Proper cleanup
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

/**
 * Mock implementation of auxiliary classes for completeness.
 */
class AwSettingsMutation {
    public Object getMutation() {
        // Return a mock mutation configuration
        return new Object(); // Placeholder implementation
    }
}

class AwActivityTestRule {
    public AwActivityTestRule(Object mutation) {
        // Constructor implementation for mutation
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(TestAwContentsClient client) {
        // Return a mock test container view
        return new AwTestContainerView(); // Placeholder
    }

    public void loadUrlAsync(AwContents awContents, String url) {
        // Mock implementation of async URL loading
    }
}

class AwTestContainerView {
    public AwContents getAwContents() {
        // Return a mock AwContents instance
        return new AwContents(); // Placeholder
    }
}

class TestAwContentsClient {
    public OnReceivedLoginRequestHelper getOnReceivedLoginRequestHelper() {
        // Return a mock login request helper
        return new OnReceivedLoginRequestHelper(); // Placeholder
    }
}

class OnReceivedLoginRequestHelper {
    private int callCount = 0;

    public int getCallCount() {
        return callCount; // Return call count
    }

    public void waitForCallback(int callCount) {
        // Simulate waiting for a callback
        this.callCount = callCount + 1; // Update call count
    }

    public String getRealm() {
        // Mock realm value
        return "com.google"; // Placeholder
    }

    public String getAccount() {
        // Mock account value
        return "foo@bar.com"; // Placeholder
    }

    public String getArgs() {
        // Mock args value
        return "random_string"; // Placeholder
    }
}
