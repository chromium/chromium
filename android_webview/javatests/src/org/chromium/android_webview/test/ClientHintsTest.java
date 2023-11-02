// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;

import java.util.Collections;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for loadUrl().
 */
@DoNotBatch(reason = "These tests conflict with each other.")
@RunWith(AwJUnit4ClassRunner.class)
public class ClientHintsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwContents mAwContents;
    private AwCookieManager mCookieManager;
    private AwEmbeddedTestServer mTestServer;
    private TestAwContentsClient mContentsClient;
    private String mLocalhostUrl;
    private String mFooUrl;

    private static final String LIGHT = "light";
    private static final String NONE = "None";

    @Before
    public void setUp() {
        mCookieManager = new AwCookieManager();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
        mTestServer = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mLocalhostUrl = mTestServer.getURL("/echoheader?sec-ch-prefers-color-scheme");
        mFooUrl = mTestServer.getURLWithHostName(
                "foo.test", "/echoheader?sec-ch-prefers-color-scheme");
    }

    @After
    public void tearDown() {
        try {
            clearCookies();
        } catch (Throwable e) {
            throw new RuntimeException("Could not clear cookies.");
        }
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.
    Add({"disable-features=" + AwFeatures.WEBVIEW_CLIENT_HINTS_CONTROLLER_DELEGATE,
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testClientHintsWithoutPersistance() throws Throwable {
        // First load of the localhost shouldn't have the hint as it wasn't requested before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        String header =
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Second load of the localhost shouldn't have the hint as it wasn't persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Clearing cookies to clear out per-origin client hint preferences.
        clearCookies();

        // Third load of the localhost shouldn't have the hint as hint prefs were cleared.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Fourth load of the localhost shouldn't have the hint as it wasn't persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // First load of foo.test shouldn't have the hint as it wasn't requested before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mFooUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Second load of foo.test shouldn't have the hint as it wasn't persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), mFooUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.
    Add({"disable-features=" + AwFeatures.WEBVIEW_CLIENT_HINTS_CONTROLLER_DELEGATE,
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testClientHintsWithPersistance() throws Throwable {
        // First load of the localhost shouldn't have the hint as it wasn't requested before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        String header =
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Second load of the localhost should have the hint as it was persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        // TODO(crbug.com/921655): Replace the expectations when this is working.
        // Assert.assertEquals(LIGHT, header);
        Assert.assertEquals(NONE, header);

        // Clearing cookies to clear out per-origin client hint preferences.
        clearCookies();

        // Third load of the localhost shouldn't have the hint as hint prefs were cleared.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Fourth load of the localhost should have the hint as it was persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        // TODO(crbug.com/921655): Replace the expectations when this is working.
        // Assert.assertEquals(LIGHT, header);
        Assert.assertEquals(NONE, header);

        // First load of foo.test shouldn't have the hint as it wasn't requested before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertEquals(NONE, header);

        // Second load of foo.test should have the hint as it was persisted before.
        loadUrlWithExtraHeadersSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                mLocalhostUrl,
                Collections.singletonMap("Accept-CH", "Sec-CH-Prefers-Color-Scheme"));
        header = mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        // TODO(crbug.com/921655): Replace the expectations when this is working.
        // Assert.assertEquals(LIGHT, header);
        Assert.assertEquals(NONE, header);
    }

    private void loadUrlWithExtraHeadersSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String url,
            final Map<String, String> extraHeaders) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> awContents.loadUrl(url, extraHeaders));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void clearCookies() throws Throwable {
        CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), mCookieManager);
    }
}
