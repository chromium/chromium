// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.Uri;

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
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSupervisedUserUrlClassifierDelegate;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
    private static final String BLOCKED_SITE_TITLE = "This website is blocked by your parent.";

    private static String makeTestPage(String title, @Nullable String iFrameUrl) {
        StringBuilder sb = new StringBuilder();
        sb.append("<html><head><title>").append(title).append("</title></head><body>Hello world!");
        if (iFrameUrl != null) {
            sb.append("<iframe id='testIframe' src='").append(iFrameUrl).append("'></iframe>");
            sb.append("<script>");
            sb.append("document.getElementById('testIframe').addEventListener('load', function(){");
            sb.append("var title;");
            sb.append("try {");
            sb.append("title = this.contentWindow.document.title");
            sb.append("} catch (error){ if (error.name == 'SecurityError') {title = '")
                    .append(BLOCKED_SITE_TITLE)
                    .append("';}}");
            sb.append("myObject.postMessage(title)});");
            sb.append("</script>");
        }
        sb.append("</body></html>");
        return sb.toString();
    }

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private OnProgressChangedClient mContentsClient = new OnProgressChangedClient();
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private IFrameLoadedListener mIFrameLoadedListener = new IFrameLoadedListener();

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge());
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAwContents.addWebMessageListener(
                    "myObject", new String[] {"*"}, mIFrameLoadedListener);
        });
    }

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testAllowedSiteIsLoaded() throws Throwable {
        String embeddedUrl = setUpWebPage(SAFE_SITE_IFRAME_PATH, SAFE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(SAFE_SITE_TITLE);
        assertIframeTitle(SAFE_SITE_IFRAME_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testDisallowedSiteIsBlocked() throws Throwable {
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, null);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testDisallowedEmbeddedSiteIsBlocked() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(SAFE_SITE_TITLE);
        assertIframeTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testDisallowedSiteRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(MATURE_SITE_PATH, SAFE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testDisallowedRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(SAFE_SITE_PATH, MATURE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("disable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK + ","
            + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void
    testDisallowedSiteIsLoadedFeatureOff() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(MATURE_SITE_TITLE);
        assertIframeTitle(MATURE_SITE_IFRAME_TITLE);
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
        String iFrameTitle = mIFrameLoadedListener.waitForResult();
        Assert.assertEquals(expectedTitle, iFrameTitle);
    }

    private static class OnProgressChangedClient extends TestAwContentsClient {
        private final List<Integer> mProgresses = new ArrayList<>();
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

    private static class IFrameLoadedListener implements WebMessageListener {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private volatile String mResult;

        @Override
        public void onPostMessage(MessagePayload payload, Uri sourceOrigin, boolean isMainFrame,
                JsReplyProxy replyProxy, MessagePort[] ports) {
            mResult = payload.getAsString();
            mCallbackHelper.notifyCalled();
        }

        public String waitForResult() throws TimeoutException {
            mCallbackHelper.waitForNext();
            return mResult;
        }
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
