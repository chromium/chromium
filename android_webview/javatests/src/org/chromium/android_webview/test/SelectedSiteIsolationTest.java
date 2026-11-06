// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;
import java.util.Set;

/** Tests for Selected Site Isolation in WebView. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(MULTI_PROCESS)
@DoNotBatch(reason = "Tests manipulate renderer processes and profile isolation state")
@CommandLineFlags.Add({
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
public class SelectedSiteIsolationTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwContents createAwContents(TestAwContentsClient contentsClient) {
        return mActivityTestRule
                .createAwTestContainerViewOnMainSync(contentsClient)
                .getAwContents();
    }

    private void configureIsolatedSitesOnUiThread(Set<String> sites) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getAwBrowserContext().configureIsolatedSites(sites));
    }

    private boolean canConfigureIsolatedSitesOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getAwBrowserContext().canConfigureIsolatedSites());
    }

    private void assertConfigureIsolatedSitesAllowedOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
                    Assert.assertTrue(context.canConfigureIsolatedSites());
                    context.configureIsolatedSites(Set.of("https://example.com"));
                    Assert.assertFalse(context.canConfigureIsolatedSites());
                });
    }

    private void assertConfigureIsolatedSitesLockedOutOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
                    Assert.assertFalse(context.canConfigureIsolatedSites());
                    Set<String> sites = Set.of("https://example.com");
                    Assert.assertThrows(
                            IllegalStateException.class,
                            () -> context.configureIsolatedSites(sites));
                });
    }

    private void assertInvalidSiteSetThrowsOnUiThread(Set<String> sites) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> context.configureIsolatedSites(sites));
                });
    }

    private static int getMainFrameProcessIdOnUiThread(AwContents awContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RenderFrameHost mainFrame = awContents.getWebContents().getMainFrame();
                    Assert.assertTrue(mainFrame.isOutermostMainFrame());
                    return mainFrame.getGlobalRenderFrameHostId().childId();
                });
    }

    private static int getIframeProcessIdOnUiThread(AwContents awContents, String expectedUrl) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RenderFrameHost mainFrame = awContents.getWebContents().getMainFrame();
                    List<RenderFrameHost> frames = mainFrame.getAllRenderFrameHosts();
                    Assert.assertEquals(2, frames.size());
                    Assert.assertEquals(mainFrame, frames.get(0));

                    RenderFrameHost iframe = frames.get(1);
                    Assert.assertFalse(iframe.isOutermostMainFrame());
                    Assert.assertEquals(expectedUrl, iframe.getLastCommittedURL().getSpec());
                    return iframe.getGlobalRenderFrameHostId().childId();
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCrossOriginIframeSharesRendererByDefault() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents = createAwContents(contentsClient);
            String originA = webServer.setServerHost("origin-a.test");
            String originB = originA.replace("origin-a.test", "origin-b.test");

            String iframeHtml = "<html><body>Iframe</body></html>";
            String iframeUrl =
                    webServer
                            .setResponse("/iframe.html", iframeHtml, null)
                            .replace(originA, originB);

            String mainHtml =
                    "<html><body><iframe src=\"" + iframeUrl + "\"></iframe></body></html>";
            String mainUrl = webServer.setResponse("/main.html", mainHtml, null);

            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), mainUrl);

            int mainFrameProcessId = getMainFrameProcessIdOnUiThread(awContents);
            int iframeProcessId = getIframeProcessIdOnUiThread(awContents, iframeUrl);
            Assert.assertEquals(mainFrameProcessId, iframeProcessId);

            Assert.assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_multipleWebViews() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            String isolatedOrigin = webServer.setServerHost("isolated.test");
            String unisolatedOrigin = isolatedOrigin.replace("isolated.test", "unisolated.test");

            configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

            String html = "<html><body>Hello</body></html>";
            String isolatedUrl = webServer.setResponse("/isolated.html", html, null);
            String unisolatedUrl =
                    webServer
                            .setResponse("/unisolated.html", html, null)
                            .replace(isolatedOrigin, unisolatedOrigin);

            TestAwContentsClient isolatedContentsClient = new TestAwContentsClient();
            AwContents isolatedAwContents = createAwContents(isolatedContentsClient);
            TestAwContentsClient unisolatedContentsClient = new TestAwContentsClient();
            AwContents unisolatedAwContents = createAwContents(unisolatedContentsClient);

            mActivityTestRule.loadUrlSync(
                    isolatedAwContents,
                    isolatedContentsClient.getOnPageFinishedHelper(),
                    isolatedUrl);
            mActivityTestRule.loadUrlSync(
                    unisolatedAwContents,
                    unisolatedContentsClient.getOnPageFinishedHelper(),
                    unisolatedUrl);

            int isolatedProcessId = getMainFrameProcessIdOnUiThread(isolatedAwContents);
            int unisolatedProcessId = getMainFrameProcessIdOnUiThread(unisolatedAwContents);
            Assert.assertNotEquals(isolatedProcessId, unisolatedProcessId);

            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_aboutBlankUsesUnisolatedRenderer() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            String isolatedOrigin = webServer.setServerHost("isolated.test");
            String unisolatedOrigin = isolatedOrigin.replace("isolated.test", "unisolated.test");

            configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

            String html = "<html><body>Hello</body></html>";
            String isolatedUrl = webServer.setResponse("/isolated.html", html, null);
            String unisolatedUrl =
                    webServer
                            .setResponse("/unisolated.html", html, null)
                            .replace(isolatedOrigin, unisolatedOrigin);

            TestAwContentsClient isolatedContentsClient = new TestAwContentsClient();
            AwContents isolatedAwContents = createAwContents(isolatedContentsClient);
            TestAwContentsClient unisolatedContentsClient = new TestAwContentsClient();
            AwContents unisolatedAwContents = createAwContents(unisolatedContentsClient);

            mActivityTestRule.loadUrlSync(
                    isolatedAwContents,
                    isolatedContentsClient.getOnPageFinishedHelper(),
                    isolatedUrl);
            mActivityTestRule.loadUrlSync(
                    unisolatedAwContents,
                    unisolatedContentsClient.getOnPageFinishedHelper(),
                    unisolatedUrl);

            int isolatedPid = getMainFrameProcessIdOnUiThread(isolatedAwContents);
            int unisolatedPid = getMainFrameProcessIdOnUiThread(unisolatedAwContents);
            Assert.assertNotEquals(isolatedPid, unisolatedPid);
            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());

            TestAwContentsClient aboutBlankContentsClient = new TestAwContentsClient();
            AwContents aboutBlankAwContents = createAwContents(aboutBlankContentsClient);
            mActivityTestRule.loadUrlSync(
                    aboutBlankAwContents,
                    aboutBlankContentsClient.getOnPageFinishedHelper(),
                    "about:blank");

            int aboutBlankPid = getMainFrameProcessIdOnUiThread(aboutBlankAwContents);
            Assert.assertEquals(unisolatedPid, aboutBlankPid);
            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_sequentialNavigations() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            String isolatedOrigin = webServer.setServerHost("isolated.test");
            String wwwIsolatedOrigin = isolatedOrigin.replace("isolated.test", "www.isolated.test");
            String unisolatedOrigin = isolatedOrigin.replace("isolated.test", "unisolated.test");
            String wwwUnisolatedOrigin =
                    isolatedOrigin.replace("isolated.test", "www.unisolated.test");

            configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents = createAwContents(contentsClient);

            String html = "<html><body>Hello</body></html>";
            String isolatedUrl = webServer.setResponse("/page.html", html, null);
            String wwwIsolatedUrl = isolatedUrl.replace(isolatedOrigin, wwwIsolatedOrigin);
            String unisolatedUrl = isolatedUrl.replace(isolatedOrigin, unisolatedOrigin);
            String wwwUnisolatedUrl = isolatedUrl.replace(isolatedOrigin, wwwUnisolatedOrigin);

            // 0. Navigate to about:blank -> unassigned renderer process.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), "about:blank");
            int pid0 = getMainFrameProcessIdOnUiThread(awContents);

            // 1. Navigate to isolated.test -> reuses the unassigned renderer and locks it to the
            // isolated shard.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), isolatedUrl);
            int pid1 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertEquals(pid0, pid1);

            // 2. Navigate to www.isolated.test -> same site, stays in the same isolated renderer.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), wwwIsolatedUrl);
            int pid2 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertEquals(pid1, pid2);

            // 3. Navigate to unisolated.test -> switches to unisolated renderer.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), unisolatedUrl);
            int pid3 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertNotEquals(pid2, pid3);

            // 4. Navigate to www.unisolated.test -> stays in the same unisolated renderer.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), wwwUnisolatedUrl);
            int pid4 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertEquals(pid3, pid4);

            // 5. Navigate back to isolated.test -> switches to isolated renderer.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), isolatedUrl);
            int pid5 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertNotEquals(pid4, pid5);

            // 6. Navigate to www.unisolated.test -> switches to unisolated renderer.
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), wwwUnisolatedUrl);
            int pid6 = getMainFrameProcessIdOnUiThread(awContents);
            Assert.assertNotEquals(pid5, pid6);
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_schemeSensitivity() throws Throwable {
        try (TestWebServer httpServer = TestWebServer.start();
                TestWebServer httpsServer = TestWebServer.startSsl()) {
            httpServer.setServerHost("a.test");
            httpsServer.setServerHost("a.test");
            String httpUrl =
                    httpServer.setResponse("/page.html", "<html><body>HTTP</body></html>", null);
            String httpsUrl =
                    httpsServer.setResponse("/page.html", "<html><body>HTTPS</body></html>", null);

            // Isolate only https://a.test, leaving http://a.test unisolated.
            configureIsolatedSitesOnUiThread(Set.of("https://a.test"));

            TestAwContentsClient httpsContentsClient = new TestAwContentsClient();
            AwContents httpsAwContents = createAwContents(httpsContentsClient);
            TestAwContentsClient httpContentsClient = new TestAwContentsClient();
            AwContents httpAwContents = createAwContents(httpContentsClient);

            mActivityTestRule.loadUrlSync(
                    httpsAwContents, httpsContentsClient.getOnPageFinishedHelper(), httpsUrl);
            mActivityTestRule.loadUrlSync(
                    httpAwContents, httpContentsClient.getOnPageFinishedHelper(), httpUrl);

            int httpsPid = getMainFrameProcessIdOnUiThread(httpsAwContents);
            int httpPid = getMainFrameProcessIdOnUiThread(httpAwContents);
            Assert.assertNotEquals(httpsPid, httpPid);
            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_loadDataWithBaseUrl() throws Throwable {
        configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

        TestAwContentsClient isolatedContentsClient = new TestAwContentsClient();
        AwContents isolatedAwContents = createAwContents(isolatedContentsClient);
        TestAwContentsClient unisolatedContentsClient = new TestAwContentsClient();
        AwContents unisolatedAwContents = createAwContents(unisolatedContentsClient);

        String html = "<html><body>Hello</body></html>";
        mActivityTestRule.loadDataWithBaseUrlSync(
                isolatedAwContents,
                isolatedContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                /* isBase64Encoded= */ false,
                "http://isolated.test/",
                /* historyUrl= */ null);
        mActivityTestRule.loadDataWithBaseUrlSync(
                unisolatedAwContents,
                unisolatedContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                /* isBase64Encoded= */ false,
                "http://unisolated.test/",
                /* historyUrl= */ null);

        int isolatedProcessId = getMainFrameProcessIdOnUiThread(isolatedAwContents);
        int unisolatedProcessId = getMainFrameProcessIdOnUiThread(unisolatedAwContents);
        Assert.assertNotEquals(isolatedProcessId, unisolatedProcessId);

        Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_mainUnisolatedIframeIsolated() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            String unisolatedOrigin = webServer.setServerHost("unisolated.test");
            String isolatedOrigin = unisolatedOrigin.replace("unisolated.test", "isolated.test");

            configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents = createAwContents(contentsClient);

            String iframeHtml = "<html><body>Iframe</body></html>";
            String iframeUrl =
                    webServer
                            .setResponse("/iframe.html", iframeHtml, null)
                            .replace(unisolatedOrigin, isolatedOrigin);

            String mainHtml =
                    "<html><body><iframe src=\"" + iframeUrl + "\"></iframe></body></html>";
            String mainUrl = webServer.setResponse("/main.html", mainHtml, null);

            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), mainUrl);

            int mainFrameProcessId = getMainFrameProcessIdOnUiThread(awContents);
            int iframeProcessId = getIframeProcessIdOnUiThread(awContents, iframeUrl);
            Assert.assertNotEquals(mainFrameProcessId, iframeProcessId);

            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSeparateRenderers_mainIsolatedIframeUnisolated() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            String isolatedOrigin = webServer.setServerHost("isolated.test");
            String unisolatedOrigin = isolatedOrigin.replace("isolated.test", "unisolated.test");

            configureIsolatedSitesOnUiThread(Set.of("http://isolated.test"));

            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents = createAwContents(contentsClient);

            String iframeHtml = "<html><body>Iframe</body></html>";
            String iframeUrl =
                    webServer
                            .setResponse("/iframe.html", iframeHtml, null)
                            .replace(isolatedOrigin, unisolatedOrigin);

            String mainHtml =
                    "<html><body><iframe src=\"" + iframeUrl + "\"></iframe></body></html>";
            String mainUrl = webServer.setResponse("/main.html", mainHtml, null);

            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), mainUrl);

            int mainFrameProcessId = getMainFrameProcessIdOnUiThread(awContents);
            int iframeProcessId = getIframeProcessIdOnUiThread(awContents, iframeUrl);
            Assert.assertNotEquals(mainFrameProcessId, iframeProcessId);

            Assert.assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsOnEmptySet() {
        assertInvalidSiteSetThrowsOnUiThread(Set.of());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsOnMalformedSite() {
        assertInvalidSiteSetThrowsOnUiThread(Set.of(""));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("*"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("not a valid site"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://*"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("://example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://ex ample.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://[::1"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsOnNonHttpOrHttpsSite() {
        assertInvalidSiteSetThrowsOnUiThread(Set.of("ftp://example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("ws://example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("wss://example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("file:///path"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("content://example.com/path"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("data:text/html,hello"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("about:blank"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("javascript:void(0)"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("custom-scheme://example.com"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsOnNonCanonicalSite() {
        // Trailing slash is not in canonical serialized form.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com/"));
        // Paths, queries, and fragments are not allowed.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com/path"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com?query=1"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com#fragment"));
        // Subdomains are not canonical sites.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://sub.example.com"));
        // Ports (both default and non-default) are not allowed in canonical sites.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com:443"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("http://example.com:80"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com:8443"));
        // Uppercase characters in scheme or host are not canonical.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("HTTPS://example.com"));
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://EXAMPLE.COM"));
        // Wildcards are not supported.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://*.example.com"));
        // Credentials are not allowed.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://user:pass@example.com"));
        // A set containing both valid and invalid sites should be rejected as a whole.
        assertInvalidSiteSetThrowsOnUiThread(Set.of("https://example.com", "https://example.com/"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_acceptsValidSites() {
        configureIsolatedSitesOnUiThread(
                Set.of(
                        "http://example.com",
                        "https://example.com",
                        "http://127.0.0.1",
                        "http://[::1]"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsWhenCalledTwice() {
        assertConfigureIsolatedSitesAllowedOnUiThread();
        assertConfigureIsolatedSitesLockedOutOnUiThread();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsAfterRendererProcessUsed() throws Throwable {
        try (TestWebServer webServer = TestWebServer.start()) {
            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents = createAwContents(contentsClient);
            Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

            String url =
                    webServer.setResponse("/main.html", "<html><body>Hello</body></html>", null);
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url);

            assertConfigureIsolatedSitesLockedOutOnUiThread();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_allowedAfterAboutBlank() throws Throwable {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwContents awContents = createAwContents(contentsClient);
        Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

        // Our documentation currently says that one should not configure after a renderer has been
        // "used". Technically, it's about whether a site has been loaded. about:blank still has a
        // renderer, but no site.
        //
        // TODO(crbug.com/576113096): This might not be the desired long-term behavior!
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), "about:blank");

        assertConfigureIsolatedSitesAllowedOnUiThread();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_allowedAfterEvaluateJavaScript() throws Throwable {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwContents awContents = createAwContents(contentsClient);
        Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

        // Evaluating JavaScript in about:blank technically doesn't change the site of the renderer,
        // so isolation can still be configured.
        //
        // TODO(crbug.com/576113096): This might not be the desired long-term behavior!
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        String result =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, contentsClient, "1 + 1");
        Assert.assertEquals("2", result);

        assertConfigureIsolatedSitesAllowedOnUiThread();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    // This prevents resource restrictions from interfering with the prerender.
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testConfigureIsolatedSites_throwsAfterPrerender() throws Throwable {
        try (TestWebServer httpsServer = TestWebServer.startSsl()) {
            AwContents awContents = createAwContents(new TestAwContentsClient());
            Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

            String httpsUrl =
                    httpsServer.setResponse("/main.html", "<html><body>Hello</body></html>", null);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Ensure that the view is visible, as prerendering cannot start in
                        // background.
                        awContents.getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
                        awContents.startPrerendering(
                                httpsUrl,
                                /* prefetchParameters= */ null,
                                /* cancellationSignal= */ null,
                                /* callbackExecutor= */ Runnable::run,
                                /* activationCallback= */ null,
                                /* errorCallback= */ null);
                    });

            CriteriaHelper.pollUiThread(
                    () -> !mActivityTestRule.getAwBrowserContext().canConfigureIsolatedSites());

            assertConfigureIsolatedSitesLockedOutOnUiThread();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_allowedAfterWarmUpSpareRenderer() {
        Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());
        // Note: we technically don't block on much here.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getAwBrowserContext().warmUpSpareRenderer());
        assertConfigureIsolatedSitesAllowedOnUiThread();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_allowedAfterPrefetch() throws Throwable {
        try (TestWebServer httpsServer = TestWebServer.startSsl()) {
            String httpsUrl =
                    httpsServer.setResponse("/page.html", "<html><body>HTTPS</body></html>", null);
            CallbackHelper prefetchCallbackHelper = new CallbackHelper();
            AwPrefetchCallback callback =
                    new AwPrefetchCallback() {
                        @Override
                        public void onStatusUpdated(
                                @StatusCode int statusCode, @Nullable Bundle extras) {
                            if (statusCode == StatusCode.PREFETCH_RESPONSE_COMPLETED) {
                                prefetchCallbackHelper.notifyCalled();
                            } else {
                                prefetchCallbackHelper.notifyFailed(
                                        "Unexpected status code: " + statusCode);
                            }
                        }

                        @Override
                        public void onError(Throwable e) {
                            prefetchCallbackHelper.notifyFailed(e.getMessage());
                        }
                    };

            Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mActivityTestRule
                                    .getAwBrowserContext()
                                    .getPrefetchManager()
                                    .startPrefetchRequest(
                                            httpsUrl,
                                            /* prefetchParameters= */ null,
                                            callback,
                                            Runnable::run));

            prefetchCallbackHelper.waitForNext();

            assertConfigureIsolatedSitesAllowedOnUiThread();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsAfterLoadData() throws Throwable {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwContents awContents = createAwContents(contentsClient);
        Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

        mActivityTestRule.loadDataSync(
                awContents,
                contentsClient.getOnPageFinishedHelper(),
                "<html><body>Hello</body></html>",
                "text/html",
                /* isBase64Encoded= */ false);

        assertConfigureIsolatedSitesLockedOutOnUiThread();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testConfigureIsolatedSites_throwsAfterLoadDataWithAboutBlankBaseUrl()
            throws Throwable {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwContents awContents = createAwContents(contentsClient);
        Assert.assertTrue(canConfigureIsolatedSitesOnUiThread());

        mActivityTestRule.loadDataWithBaseUrlSync(
                awContents,
                contentsClient.getOnPageFinishedHelper(),
                "<html><body>Hello</body></html>",
                "text/html",
                /* isBase64Encoded= */ false,
                "about:blank",
                /* historyUrl= */ null);

        assertConfigureIsolatedSitesLockedOutOnUiThread();
    }
}
