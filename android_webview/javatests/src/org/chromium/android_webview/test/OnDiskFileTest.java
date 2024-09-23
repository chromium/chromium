// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;

/** Test suite for files WebView creates on disk. This includes HTTP cache and the cookies file. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class OnDiskFileTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public OnDiskFileTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        // We need to control when the browser process starts, so that we can delete
                        // the file-under-test before the test starts up.
                        return false;
                    }
                };
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpCacheIsInsideCacheDir() throws Exception {
        File webViewCacheDir =
                new File(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getCacheDir()
                                .getPath(),
                        "WebView/Default/HTTP Cache");
        FileUtils.recursivelyDeleteFile(webViewCacheDir, FileUtils.DELETE_ALL);

        mActivityTestRule.startBrowserProcess();
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer httpServer = null;
        try {
            httpServer = TestWebServer.start();
            final String pageUrl = "/page.html";
            final String pageHtml = "<body>Hello, World!</body>";
            final String fullPageUrl = httpServer.setResponse(pageUrl, pageHtml, null);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), fullPageUrl);
            Assert.assertEquals(1, httpServer.getRequestCount(pageUrl));
        } finally {
            if (httpServer != null) {
                httpServer.shutdown();
            }
        }

        Assert.assertTrue(webViewCacheDir.isDirectory());
        Assert.assertTrue(webViewCacheDir.list().length > 0);
    }

    @Test
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @Feature({"AndroidWebView"})
    public void testCookiePathIsInsideDataDir() {
        File webViewCookiePath =
                new File(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getDir("webview", Context.MODE_PRIVATE)
                                .getPath(),
                        "Default/Cookies");
        webViewCookiePath.delete();

        // Set a cookie and flush it to disk. This should guarantee the cookie file is created.
        final AwCookieManager cookieManager = new AwCookieManager();
        final String url = "http://www.example.com";
        cookieManager.setCookie(url, "key=value");
        cookieManager.flushCookieStore();

        Assert.assertTrue(webViewCookiePath.isFile());
    }

    @Test
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    @Feature({"AndroidWebView"})
    public void testProfilesHaveSeparateDirectories() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        // Check Default uses its own constant directory.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Default",
                            AwBrowserContextStore.getNamedContextPathForTesting("Default"));
                });

        // Check NonDefaults use "Profile 1", "Profile 2", ...
        final int numProfiles = 2;
        for (int profile = 1; profile <= numProfiles; profile++) {
            final String contextName = "MyAwesomeProfile" + profile;
            final String relativePath = "Profile " + profile;

            final File contextPath =
                    new File(
                            InstrumentationRegistry.getInstrumentation()
                                    .getTargetContext()
                                    .getDir("webview", Context.MODE_PRIVATE)
                                    .getPath(),
                            relativePath);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        contextPath.delete();

                        AwBrowserContextStore.getNamedContext(
                                contextName, /* createIfNeeded= */ true);

                        Assert.assertEquals(
                                relativePath,
                                AwBrowserContextStore.getNamedContextPathForTesting(contextName));
                        Assert.assertTrue(contextPath.isDirectory());
                    });
        }
    }
}
