// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasHost;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasParamWithValue;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasPath;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.net.Uri;

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
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.common.AwSupervisedUserUrlClassifierDelegate;
import org.chromium.android_webview.common.BackgroundThreadExecutor;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.settings.SpeculativeLoadingAllowedFlags;
import org.chromium.android_webview.supervised_user.AwSupervisedUserSafeModeAction;
import org.chromium.android_webview.supervised_user.AwSupervisedUserUrlClassifier;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
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
    private static final String SUPPORT_CENTER_PLINK = "content_blocked_webview";

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

    private final OnProgressChangedClient mContentsClient = new OnProgressChangedClient();
    private final TestAwSupervisedUserUrlClassifierDelegate mDelegate =
            new TestAwSupervisedUserUrlClassifierDelegate();
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private final TestWebMessageListener mIframeLoadedListener = new TestWebMessageListener();

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
                            "myObject", new String[] {"*"}, mIframeLoadedListener);
                });
        resetNeedsRestriction(true);
    }

    @After
    public void tearDown() throws Exception {
        if (mAwContents != null) {
            mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        }
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        AwSupervisedUserSafeModeAction.resetForTesting();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
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
    public void testDisallowedSiteIsBlocked() throws Throwable {
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, null);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
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
    public void testDisallowedSiteRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(MATURE_SITE_PATH, SAFE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDisallowedRedirectIsBlocked() throws Throwable {
        String requestUrl = mWebServer.setRedirect(SAFE_SITE_PATH, MATURE_SITE_PATH);

        loadUrl(requestUrl);

        assertPageTitle(BLOCKED_SITE_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
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
                    Criteria.checkThat(
                            mActivityTestRule.getTitleOnUiThread(mAwContents),
                            Matchers.is(BLOCKED_SITE_TITLE));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
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
                            hasData(hasPath(SUPPORT_CENTER_PATH)),
                            hasData(hasParamWithValue("p", SUPPORT_CENTER_PLINK))));
        } finally {
            Intents.release();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeMode() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        new AwSupervisedUserSafeModeAction().executeAtStartup();
        loadUrl(requestUrl);

        assertPageTitle(MATURE_SITE_TITLE);
        assertIframeTitle(MATURE_SITE_IFRAME_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPrerenderDisallowedSiteIsBlocked() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mAwContents)
                .setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);

        String matureUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, null);
        String safeUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, null);

        loadUrl(safeUrl);
        assertPageTitle(SAFE_SITE_TITLE);

        injectSpeculationRules(matureUrl);
        loadUrl(matureUrl);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getTitleOnUiThread(mAwContents),
                            Matchers.is(BLOCKED_SITE_TITLE));
                });

        // If we get this far, then it means the page was correctly blocked (and prerender didn't
        // cause the page to slip through navigation).

        Assert.assertEquals(
                "The unsafe test site should be blocked before the prerender starts",
                0,
                mWebServer.getRequestCount(MATURE_SITE_PATH));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeSitesCanBePrerendered() throws Throwable {
        final TestWebMessageListener prerenderStatusListener = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.addWebMessageListener(
                            "prerenderStatusListener", new String[] {"*"}, prerenderStatusListener);
                });

        mActivityTestRule
                .getAwSettingsOnUiThread(mAwContents)
                .setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);

        String prerenderSafeUrl = setUpPrerenderSafePage();
        String safeUrl = setUpWebPage(SAFE_SITE_PATH, SAFE_SITE_TITLE, null);

        loadUrl(safeUrl);
        assertPageTitle(SAFE_SITE_TITLE);

        injectSpeculationRules(prerenderSafeUrl);

        // We poll for the subresource request "/prerendered_ready.png" to ensure the main page has
        // been fully downloaded, parsed, and has started prerendering. If we don't wait for
        // prerendering to start, then the prerender and loadUrl() call will race and this may not
        // count as a prerendered navigation, which defeats the purpose of this test case.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mWebServer.getRequestCount("/prerendered_ready.png"),
                            Matchers.greaterThan(0));
                });

        // Verify that we can still navigate to the safe page.
        loadUrl(prerenderSafeUrl);
        assertPageTitle("Prerender Safe site");

        // And verify that this navigation was for a page that was prerendered.
        Assert.assertEquals("prerendered_and_activated", prerenderStatusListener.waitForResult());
    }

    private String setUpPrerenderSafePage() {
        mWebServer.setResponseWithNoContentStatus("/prerendered_ready.png");
        String content =
                """
                <html>
                <head>
                <title>Prerender Safe site</title>
                <script>
                  window.wasPrerendered = document.prerendering;
                  if (document.prerendering) {
                    document.addEventListener('prerenderingchange', function() {
                      prerenderStatusListener.postMessage("prerendered_and_activated");
                    });
                  } else {
                    prerenderStatusListener.postMessage("not_prerendered");
                  }
                </script>
                </head>
                <body>
                  <h1>Prerender Safe site</h1>
                  <img src="/prerendered_ready.png">
                </body>
                </html>
                """;
        return mWebServer.setResponse("/prerender-safe.html", content, null);
    }

    private void injectSpeculationRules(String url) throws Exception {
        final String speculationRulesTemplate =
                """
                {
                  const script = document.createElement('script');
                  script.type = 'speculationrules';
                  script.text = '{"prerender": [{"source": "list", "urls": ["%s"]}]}';
                  document.head.appendChild(script);
                }
                """;
        final String speculationRules = String.format(speculationRulesTemplate, url);
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, speculationRules);
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

    private void assertPageTitle(String expectedTitle) {
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    private void assertIframeTitle(String expectedTitle) throws Exception {
        String iFrameTitle = mIframeLoadedListener.waitForResult();
        Assert.assertEquals(expectedTitle, iFrameTitle);
    }

    private static class OnProgressChangedClient extends TestAwContentsClient {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private int mLastProgress = -1;

        @Override
        public void onProgressChanged(int progress) {
            super.onProgressChanged(progress);
            if (progress == 100 && mLastProgress != 100) {
                mCallbackHelper.notifyCalled();
            }
            mLastProgress = progress;
        }

        public void waitForFullLoad() throws TimeoutException {
            mCallbackHelper.waitForNext();
        }
    }

    private static class TestWebMessageListener implements WebMessageListener {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private volatile @Nullable String mResult;

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

        public @Nullable String waitForResult() throws TimeoutException {
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
        private @Nullable Boolean mNeedsRestrictionResponse;
        private static final Set RESTRICTED_CONTENT_BLOCKLIST =
                Set.of(MATURE_SITE_PATH, MATURE_SITE_IFRAME_PATH);

        @Override
        public void shouldBlockUrl(GURL requestUrl, final Callback<Boolean> callback) {
            String path = requestUrl.getPath();
            boolean isRestrictedContent = RESTRICTED_CONTENT_BLOCKLIST.contains(path);
            mExecutor.execute(
                    () -> {
                        callback.onResult(isRestrictedContent);
                    });
        }

        @Override
        public void needsRestrictedContentBlocking(final Callback<@Nullable Boolean> callback) {
            mExecutor.execute(
                    () -> {
                        callback.onResult(mNeedsRestrictionResponse);
                        mNeedsRestrictionHelper.notifyCalled();
                    });
        }

        public void setNeedsRestrictedContentBlockingResponse(@Nullable Boolean value) {
            mNeedsRestrictionResponse = value;
        }

        public CallbackHelper getNeedsRestrictionHelper() {
            return mNeedsRestrictionHelper;
        }
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        final AwSupervisedUserUrlClassifierDelegate mDelegate;

        public TestPlatformServiceBridge(AwSupervisedUserUrlClassifierDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public AwSupervisedUserUrlClassifierDelegate getUrlClassifierDelegate() {
            return mDelegate;
        }
    }

    private void resetNeedsRestriction(@Nullable Boolean value) throws Exception {
        mDelegate.setNeedsRestrictedContentBlockingResponse(value);
        int count = mDelegate.getNeedsRestrictionHelper().getCallCount();
        AwSupervisedUserUrlClassifier classifier = AwSupervisedUserUrlClassifier.getInstance();
        Assert.assertNotNull("Must set a classifier for this test class to run.", classifier);

        classifier.checkIfNeedRestrictedContentBlocking();
        mDelegate.getNeedsRestrictionHelper().waitForCallback(count);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRestrictedContentBlockingNullDoesNotUpdateCache() throws Throwable {
        String embeddedUrl = setUpWebPage(MATURE_SITE_IFRAME_PATH, MATURE_SITE_IFRAME_TITLE, null);
        String requestUrl = setUpWebPage(MATURE_SITE_PATH, MATURE_SITE_TITLE, embeddedUrl);

        // Start with restriction enabled (setUp sets it to true, but let's be explicit)
        resetNeedsRestriction(true);
        loadUrl(requestUrl);
        assertPageTitle(BLOCKED_SITE_TITLE);

        // Now set restriction response to null (simulating timeout/error)
        // It should NOT update the cache, so restriction should remain enabled (mature pages
        // blocked)
        // We also check that the histogram is NOT recorded.
        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.WebView.RestrictedContentBlocking.ApiCallMatchesDiskCache")
                        .build()) {
            resetNeedsRestriction(null);
            loadUrl(requestUrl);
            assertPageTitle(BLOCKED_SITE_TITLE);
        }

        // Now set restriction to false
        resetNeedsRestriction(false);
        loadUrl(requestUrl);
        assertPageTitle(MATURE_SITE_TITLE);

        // Now set restriction response to null again
        // It should NOT update the cache, so restriction should remain disabled (mature pages
        // allowed)
        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.WebView.RestrictedContentBlocking.ApiCallMatchesDiskCache")
                        .build()) {
            resetNeedsRestriction(null);
            loadUrl(requestUrl);
            assertPageTitle(MATURE_SITE_TITLE);
        }
    }
}
