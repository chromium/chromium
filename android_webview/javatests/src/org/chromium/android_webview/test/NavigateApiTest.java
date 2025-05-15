// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationParams;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for AwContents#navigate. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class NavigateApiTest extends AwParameterizedTest {
    private static final String PAGE1_PATH = "/index.html";
    private static final String PAGE2_PATH = "/example.html";

    private static final String HEADER_NAME = "MyHeader";
    private static final String HEADER_VALUE = "MyValue";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;

    private TestWebServer mWebServer;
    private String mPage1Url;
    private String mPage2Url;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private final TestAwNavigationClient mNavigationClient = new TestAwNavigationClient();
    private CallbackHelper mOnPageLoadFinished;

    public NavigateApiTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwTestContainerView container =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = container.getAwContents();
        mAwContents.setNavigationClient(mNavigationClient);

        mOnPageLoadFinished = mContentsClient.getOnPageFinishedHelper();

        mWebServer = TestWebServer.start();
        mPage1Url = mWebServer.setResponse(PAGE1_PATH, "<html><body>foo</body></html>", null);
        mPage2Url = mWebServer.setResponse(PAGE2_PATH, "<html><body>bar</body></html>", null);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void navigates() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate...
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));

        // ... results in a page load.
        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void navigationCallback() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();
        AtomicReference<AwNavigation> navigationRef = new AtomicReference<>();

        // Tests that the AwNavigation object returned by navigate...
        ThreadUtils.runOnUiThreadBlocking(() -> navigationRef.set(mAwContents.navigate(mPage1Url)));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

        // ... is the same as the one provided to the navigation callbacks.
        AwNavigation navigation = navigationRef.get();
        Assert.assertNotNull(navigation);
        Assert.assertSame(navigation, mNavigationClient.getLastStartedNavigation());
        Assert.assertSame(navigation, mNavigationClient.getLastCompletedNavigation());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void shouldReplaceCurrentEntry_false() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Tests that calling navigate twice with default arguments...
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage2Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE2_PATH));

        // ... results in two history entries.
        NavigationHistory history = mAwContents.getNavigationHistory();
        Assert.assertEquals(2, history.getEntryCount());

        Assert.assertTrue(mAwContents.canGoBack());
    }

    private static AwNavigationParams paramsWithReplaceCurrentEntry(String url) {
        boolean shouldReplaceCurrentEntry = true;
        return new AwNavigationParams(url, shouldReplaceCurrentEntry);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void shouldReplaceCurrentEntry_true() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate twice (with shouldReplaceCurrentEntry = true)...
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.navigate(paramsWithReplaceCurrentEntry(mPage1Url)));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.navigate(paramsWithReplaceCurrentEntry(mPage2Url)));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE2_PATH));

        // ... results in only one history entry.
        NavigationHistory history = mAwContents.getNavigationHistory();
        Assert.assertEquals(1, history.getEntryCount());

        Assert.assertFalse(mAwContents.canGoBack());
    }
}
