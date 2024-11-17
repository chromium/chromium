// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * Tests for CookieManager/Chromium startup ordering weirdness.
 *
 * <p>This tests various cases around ordering of calls to CookieManager at startup, and thus is
 * separate from the normal CookieManager tests so it can control call ordering carefully.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class CookieManagerStartupTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public CookieManagerStartupTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsAwBrowserContextCreated() {
                        return false;
                    }

                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Before
    public void setUp() {
        // CookieManager assumes that native is loaded, but webview browser should not be loaded for
        // these tests as webview is not necessarily loaded when CookieManager is called.
        AwBrowserProcess.loadLibrary(null);
    }

    /**
     * Called when a test wants to initiate normal Chromium process startup, after
     * doing any CookieManager calls that are supposed to happen before the UI thread
     * is committed.
     */
    private void startChromium() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
        startChromiumWithClient(new TestAwContentsClient());
    }

    /**
     * Called when a test wants to initiate normal Chromium process startup, after
     * doing any CookieManager calls that are supposed to happen before the UI thread
     * is committed.
     */
    private void startChromiumWithClient(TestAwContentsClient contentsClient) {
        mActivityTestRule.createAwBrowserContext();
        mActivityTestRule.startBrowserProcess();
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("disable-partitioned-cookies")
    public void testStartup() throws Throwable {
        ThreadUtils.setWillOverrideUiThread();
        EmbeddedTestServer webServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK);
        try {
            String url = webServer.getURL("/android_webview/test/data/hello_world.html");

            // Verify that we can use AwCookieManager successfully before having started Chromium.
            AwCookieManager cookieManager = new AwCookieManager();
            Assert.assertNotNull(cookieManager);

            CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), cookieManager);
            Assert.assertFalse(cookieManager.hasCookies());

            cookieManager.setAcceptCookie(true);
            Assert.assertTrue(cookieManager.acceptCookie());

            cookieManager.setCookie(url, "count=41");
            cookieManager.setCookie(url, "partitioned_cookie=123;Secure;Partitioned");

            // Now start Chromium to cause the switch from the temporary cookie store to the real
            // Mojo store.
            startChromium();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents,
                    mContentsClient,
                    "var c=document.cookie.split('=');"
                            + "document.cookie=c[0]+'='+(1+(+c[1].split(';')[0]));");

            // Verify that the cookie value we set before was successfully passed through to the
            // Mojo store.
            Assert.assertEquals("partitioned_cookie=123; count=42", cookieManager.getCookie(url));
        } finally {
            webServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @Feature({"AndroidWebView", "Privacy"})
    public void testAllowFileSchemeCookies() {
        AwCookieManager cookieManager = new AwCookieManager();
        Assert.assertFalse(cookieManager.allowFileSchemeCookies());
        cookieManager.setAcceptFileSchemeCookies(true);
        Assert.assertTrue(cookieManager.allowFileSchemeCookies());
        cookieManager.setAcceptFileSchemeCookies(false);
        Assert.assertFalse(cookieManager.allowFileSchemeCookies());
    }

    @Test
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @Feature({"AndroidWebView", "Privacy"})
    public void testAllowCookies() {
        AwCookieManager cookieManager = new AwCookieManager();
        Assert.assertTrue(cookieManager.acceptCookie());
        cookieManager.setAcceptCookie(false);
        Assert.assertFalse(cookieManager.acceptCookie());
        cookieManager.setAcceptCookie(true);
        Assert.assertTrue(cookieManager.acceptCookie());
    }

    // https://code.google.com/p/chromium/issues/detail?id=374203
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testShouldInterceptRequestDeadlock() throws Throwable {
        ThreadUtils.setWillOverrideUiThread();
        ThreadUtils.setUiThread(Looper.getMainLooper());
        String url = "http://www.example.com";
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        new AwCookieManager().getCookie("www.example.com");
                        return null;
                    }
                };
        startChromiumWithClient(contentsClient);
        mActivityTestRule.loadUrlSync(mAwContents, contentsClient.getOnPageFinishedHelper(), url);
    }
}
