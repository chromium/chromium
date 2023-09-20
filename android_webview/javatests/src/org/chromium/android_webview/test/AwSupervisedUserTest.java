// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSupervisedUserUrlClassifierDelegate;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for blocking mature sites for supervised users.
 *
 * These tests only check the url loading part of the integration, not
 * the call to GMS core which would check if the current user can load
 * a particular url.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AwSupervisedUserTest {
    private static final String SAFE_SITE_TITLE = "Safe site";
    private static final String SAFE_SITE_PATH = "/safe.html";
    private static final String SAFE_SITE_IFRAME_TITLE = "IFrame safe site";
    private static final String SAFE_SITE_IFRAME_PATH = "/safe-inner.html";
    private static final String MATURE_SITE_TITLE = "Mature site";
    private static final String MATURE_SITE_PATH = "/mature.html";
    private static final String MATURE_SITE_IFRAME_TITLE = "IFrame mature site";
    private static final String MATURE_SITE_IFRAME_PATH = "/mature-inner.html";
    private static final String BLOCKED_SITE_TITLE = "Webpage not available";

    private static String makeTestPage(String title, @Nullable String iFrameUrl) {
        return "<html>"
                + "  <head>"
                + "    <title>" + title + "</title>"
                + "  </head>"
                + "  <body>"
                + "    Hello world!"
                + ((iFrameUrl != null) ? ("<iframe src=\"" + iFrameUrl + "\"/iframe>") : "")
                + "  </body>"
                + "</html>";
    }

    private static class OnProgressChangedClient extends TestAwContentsClient {
        private final List<Integer> mProgresses = new ArrayList<Integer>();
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onProgressChanged(int progress) {
            super.onProgressChanged(progress);
            mProgresses.add(Integer.valueOf(progress));
            if (progress == 100 && mCallbackHelper.getCallCount() == 0) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForFullLoad() throws TimeoutException {
            mCallbackHelper.waitForFirst();
        }
    }

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private OnProgressChangedClient mContentsClient = new OnProgressChangedClient();
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge());
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testAllowedSiteIsLoaded() throws Throwable {
        String embeddedUrl = setUpWebPage(SAFE_SITE_IFRAME_PATH, SAFE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(SAFE_SITE_TITLE);
        // todo(jdeabreu): fix flaky iframe test check
        // assertIframeTitle(SAFE_SITE_IFRAME_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testDisallowedSiteIsBlocked() throws Throwable {
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, null);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testDisallowedEmbeddedSiteIsBlocked() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(SAFE_SITE_TITLE);
        // todo(jdeabreu): fix flaky iframe test check
        // assertIframeTitle("null");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testDisallowedSiteRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(MATURE_SITE_PATH, SAFE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testDisallowedRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(SAFE_SITE_PATH, MATURE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    public void testDisallowedSiteIsLoadedFeatureOff() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(MATURE_SITE_TITLE);
        // todo(jdeabreu): fix flaky iframe test check
        // assertIframeTitle(MATURE_SITE_IFRAME_TITLE);
    }

    private String setUpWebPage(String path, String title, @Nullable String iFrameUrl) {
        return mWebServer.setResponse(path, makeTestPage(title, iFrameUrl), null);
    }

    private void loadUrl(String requestUrl) throws TimeoutException {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mAwContents.loadUrl(requestUrl, null));
        mContentsClient.waitForFullLoad();
    }

    private void assertPageTitle(String expectedTitle) {
        Assert.assertEquals(expectedTitle, mAwContents.getTitle());
    }

    private void assertIframeTitle(String expectedTitle) throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        String iFrameTitle = getJavaScriptResultIframeTitle(mAwContents, mContentsClient);
        Assert.assertEquals(expectedTitle, iFrameTitle);
    }

    /**
     * Like {@link AwActivityTestRule#getJavaScriptResultBodyTextContent}, but it gets the title
     * of the iframe instead. This assumes the main frame has only a single iframe.
     */
    private String getJavaScriptResultIframeTitle(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        String script = "document.getElementsByTagName('iframe')[0].contentDocument.title";
        String raw =
                mActivityTestRule.executeJavaScriptAndWaitForResult(awContents, viewClient, script);
        return mActivityTestRule.maybeStripDoubleQuotes(raw);
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private class TestAwSupervisedUserUrlClassifierDelegate
                implements AwSupervisedUserUrlClassifierDelegate {
            @Override
            public void shouldBlockUrl(GURL requestUrl, @NonNull final Callback<Boolean> callback) {
                String path = requestUrl.getPath();

                if (path.equals(SAFE_SITE_PATH) || path.equals(SAFE_SITE_IFRAME_PATH)) {
                    callback.onResult(false);
                    return;
                } else if (path.equals(MATURE_SITE_PATH) || path.equals(MATURE_SITE_IFRAME_PATH)) {
                    callback.onResult(true);
                    return;
                }
                assert false;
            }
        }

        @Override
        public AwSupervisedUserUrlClassifierDelegate getUrlClassifierDelegate() {
            return new TestAwSupervisedUserUrlClassifierDelegate();
        }
    }
}
