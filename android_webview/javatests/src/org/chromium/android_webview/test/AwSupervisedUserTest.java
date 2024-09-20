// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasHost;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasPath;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSupervisedUserUrlClassifierDelegate;
import org.chromium.android_webview.common.BackgroundThreadExecutor;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.supervised_user.AwSupervisedUserUrlClassifier;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeoutException;

/**
 * Tests for blocking mature sites for supervised users.
 *
 * <p>These tests only check the url loading part of the integration, not the call to GMS core which
 * would check if the current user can load a particular url.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwSupervisedUserTest extends AwParameterizedTest {
    private static final String SAFE_SITE_TITLE = "Safe site";
    private static final String SAFE_SITE_PATH = "/safe.html";
    private static final String SAFE_SITE_IFRAME_TITLE = "IFrame safe site";
    private static final String SAFE_SITE_IFRAME_PATH = "/safe-inner.html";
    private static final String MATURE_SITE_TITLE = "Mature site";
    private static final String MATURE_SITE_PATH = "/mature.html";
    private static final String MATURE_SITE_IFRAME_TITLE = "IFrame mature site";
    private static final String MATURE_SITE_IFRAME_PATH = "/mature-inner.html";
    private static final String BLOCKED_SITE_TITLE = "This content is blocked.";
    private static final String LEARN_MORE_LINK = "learn-more-link";
    private static final String SUPPORT_CENTER_HOST = "support.google.com";
    private static final String SUPPORT_CENTER_PATH = "/families";

    private static String makeTestPage(String title, @Nullable String iFrameUrl) {
        StringBuilder sb = new StringBuilder();
        sb.append("<html><head><title>").append(title).append("</title></head>");
        sb.append("<body>");
        sb.append("<h1>").append(title).append("</h1>");
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

    @Rule public AwActivityTestRule mActivityTestRule;

    private OnProgressChangedClient mContentsClient = new OnProgressChangedClient();
    private TestAwSupervisedUserUrlClassifierDelegate mDelegate =
            new TestAwSupervisedUserUrlClassifierDelegate();
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private IFrameLoadedListener mIFrameLoadedListener = new IFrameLoadedListener();

    public AwSupervisedUserTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();

        // The Classifier is initially set in AwBrowserProcess#start(). We need to reset this so
        // that we get a fresh Classifier that uses our TestAwSupervisedUserUrlClassifierDelegate.
        AwSupervisedUserUrlClassifier.resetInstanceForTesting();

        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(mDelegate));
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.addWebMessageListener(
                            "myObject", new String[] {"*"}, mIFrameLoadedListener);
                });
        resetNeedsRestriction(true);
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
    @CommandLineFlags.Add(
            "disable-features="
                    + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK
                    + ","
                    + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
    public void testDisallowedSiteIsLoadedFeatureOff() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        loadUrl(requestUrl);

        assertPageTitle(MATURE_SITE_TITLE);
        assertIframeTitle(MATURE_SITE_IFRAME_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testBlocksContentOnlyIfRestrctionRequired() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        // If the user does not require content restriction, then the pages should load fully.
        resetNeedsRestriction(false);
        loadUrl(requestUrl);
        assertPageTitle(MATURE_SITE_TITLE);
        assertIframeTitle(MATURE_SITE_IFRAME_TITLE);

        // If the user requires content restriction, then the pages should be blocked.
        resetNeedsRestriction(true);
        loadUrl(requestUrl);
        // The page title updates after waitForFullLoad, so we don't have a guarantee yet that the
        // page title has updated. WebView doesn't have callbacks for page title change, so polling
        // is the best option.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mActivityTestRule.getTitleOnUiThread(mAwContents),
                                Matchers.is(BLOCKED_SITE_TITLE));
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("enable-features=" + AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)
    public void testClickLearnMoreLink() throws Throwable {
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, null);
        try {
            Intents.init();
            intending(not(IntentMatchers.isInternal()))
                    .respondWith(new ActivityResult(Activity.RESULT_OK, null));

            loadUrl(requestUrl);
            assertPageTitle(BLOCKED_SITE_TITLE);

            final String script = "document.getElementById('" + LEARN_MORE_LINK + "').click();";
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, script);
            CriteriaHelper.pollInstrumentationThread(
                    () -> Intents.getIntents().size() > 0, "An intent should be received");
            intended(
                    allOf(
                            hasAction(Intent.ACTION_VIEW),
                            hasData(hasHost(SUPPORT_CENTER_HOST)),
                            hasData(hasPath(SUPPORT_CENTER_PATH))));
        } finally {
            Intents.release();
        }
    }

    private String setUpWebPage(String path, String title, @Nullable String iFrameUrl) {
        return mWebServer.setResponse(path, makeTestPage(title, iFrameUrl), null);
    }

    private void loadUrl(String requestUrl) throws Exception {
        // If the page is blocked, then it will never fire the onPageFinished callback so we can't
        // use loadUrlSync(). Instead, use loadUrlAsync and wait for onProgressChanged().
        mActivityTestRule.loadUrlAsync(mAwContents, requestUrl);
        mContentsClient.waitForFullLoad();
    }

    private void assertPageTitle(String expectedTitle) throws Exception {
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    private void assertIframeTitle(String expectedTitle) throws Exception {
        String iFrameTitle = mIFrameLoadedListener.waitForResult();
        Assert.assertEquals(expectedTitle, iFrameTitle);
    }

    private static class OnProgressChangedClient extends TestAwContentsClient {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onProgressChanged(int progress) {
            super.onProgressChanged(progress);
            if (progress == 100 && mCallbackHelper.getCallCount() == 0) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForFullLoad() throws TimeoutException {
            mCallbackHelper.waitForOnly();
        }
    }

    private static class IFrameLoadedListener implements WebMessageListener {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private volatile String mResult;

        @Override
        public void onPostMessage(
                MessagePayload payload,
                Uri topLevelOrigin,
                Uri sourceOrigin,
                boolean isMainFrame,
                JsReplyProxy replyProxy,
                MessagePort[] ports) {
            mResult = payload.getAsString();
            mCallbackHelper.notifyCalled();
        }

        public String waitForResult() throws TimeoutException {
            mCallbackHelper.waitForNext();
            return mResult;
        }
    }

    private static class TestAwSupervisedUserUrlClassifierDelegate
            implements AwSupervisedUserUrlClassifierDelegate {
        // Post callback responses to a background thread to emulate how the production code
        // works.
        private final Executor mExecutor = new BackgroundThreadExecutor("TEST_BACKGROUND_THREAD");
        private final CallbackHelper mNeedsRestrictionHelper = new CallbackHelper();
        private boolean mNeedsRestrictionResponse;
        private static final Set RESTRICTED_CONTENT_BLOCKLIST =
                Set.of(MATURE_SITE_PATH, MATURE_SITE_IFRAME_PATH);

        @Override
        public void shouldBlockUrl(GURL requestUrl, @NonNull final Callback<Boolean> callback) {
            String path = requestUrl.getPath();
            boolean isRestrictedContent = RESTRICTED_CONTENT_BLOCKLIST.contains(path);
            mExecutor.execute(
                    () -> {
                        callback.onResult(isRestrictedContent);
                    });
        }

        @Override
        public void needsRestrictedContentBlocking(@NonNull final Callback<Boolean> callback) {
            mExecutor.execute(
                    () -> {
                        callback.onResult(mNeedsRestrictionResponse);
                        mNeedsRestrictionHelper.notifyCalled();
                    });
        }

        public void setNeedsRestrictedContentBlockingResponse(boolean value) {
            mNeedsRestrictionResponse = value;
        }

        public CallbackHelper getNeedsRestrictionHelper() {
            return mNeedsRestrictionHelper;
        }
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        AwSupervisedUserUrlClassifierDelegate mDelegate;

        public TestPlatformServiceBridge(AwSupervisedUserUrlClassifierDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public AwSupervisedUserUrlClassifierDelegate getUrlClassifierDelegate() {
            return mDelegate;
        }
    }

    private void resetNeedsRestriction(boolean value) throws Exception {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION)
                && !AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK)) {
            // Nothing we need to do if the feature is disabled.
            return;
        }
        mDelegate.setNeedsRestrictedContentBlockingResponse(value);
        int count = mDelegate.getNeedsRestrictionHelper().getCallCount();
        AwSupervisedUserUrlClassifier classifier = AwSupervisedUserUrlClassifier.getInstance();
        Assert.assertNotNull("Must set a classifier for this test class to run.", classifier);

        classifier.checkIfNeedRestrictedContentBlocking();
        mDelegate.getNeedsRestrictionHelper().waitForCallback(count);
    }
}
