// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests that the variations headers are correctly set.
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.
Add({"disable-field-trial-config", "force-variation-ids=4,10,34", "host-rules=MAP * 127.0.0.1",
        "ignore-certificate-errors", "enable-features=WebViewSendVariationsHeaders"})
public class VariationsHeadersTest {
    private static final String PATH = "/ok.html";
    private static final String HEADER_NAME = "X-Client-Data";

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private String mUrl;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mTestServer;

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private String mPackageId;

        public TestPlatformServiceBridge(String packageId) {
            mPackageId = packageId;
        }

        @Override
        public String getFirstPartyVariationsHeadersEnabledPackageId() {
            return mPackageId;
        }
    }

    @Before
    public void setUp() throws Throwable {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mTestServer = TestWebServer.startSsl();
        mTestServer.setServerHost("google.com");
        mUrl = mTestServer.setResponse(PATH, "<html>ok</html>", null);

        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(
                ContextUtils.getApplicationContext().getPackageName()));
    }

    @After
    public void tearDown() {
        mTestServer.shutdown();
    }

    @MediumTest
    @Test
    public void testSendsHeaderWithFeatureEnabled() throws Throwable {
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mUrl);
        Assert.assertFalse(mTestServer.getLastRequest(PATH).headerValue(HEADER_NAME).isEmpty());
    }

    @MediumTest
    @Test
    public void testMatchesApiValue() throws Throwable {
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mUrl);
        String serverHeaderValue = mTestServer.getLastRequest(PATH).headerValue(HEADER_NAME);
        Assert.assertEquals(serverHeaderValue, AwContentsStatics.getVariationsHeader());
    }

    @CommandLineFlags.Add({"disable-features=WebViewSendVariationsHeaders"})
    @MediumTest
    @Test
    public void testDoesNotSendHeaderWithFeatureDisabled() throws Throwable {
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mUrl);
        Assert.assertEquals("", mTestServer.getLastRequest(PATH).headerValue(HEADER_NAME));
    }

    @MediumTest
    @Test
    public void testDoesNotSendHeaderForOtherHost() throws Throwable {
        TestWebServer testServer = TestWebServer.startAdditionalSsl();
        testServer.setServerHost("example.com");
        String url = testServer.setResponse(PATH, "<html>ok</html>", null);
        try {
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals("", testServer.getLastRequest(PATH).headerValue(HEADER_NAME));
        } finally {
            testServer.shutdown();
        }
    }

    @MediumTest
    @Test
    public void testDoesNotSendHeaderWhenPackageIdIncorrect() throws Throwable {
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge("foo.bar"));
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mUrl);
        Assert.assertEquals("", mTestServer.getLastRequest(PATH).headerValue(HEADER_NAME));
    }
}
