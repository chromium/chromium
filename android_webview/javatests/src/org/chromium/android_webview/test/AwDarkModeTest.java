// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.DarkModeHelper;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * The integration test for the dark mode.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
@SuppressLint("NewApi")
public class AwDarkModeTest {
    private static final String FILE = "/main.html";
    private static final String DATA =
            "<html><head><meta name=\"color-scheme\" content=\"dark light\"></head>"
            + "<body>DarkMode</body></html>";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private CallbackHelper mCallbackHelper = new CallbackHelper();
    private AwContents mAwContents;

    @Before
    public void setUp() throws Exception {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(
                mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLightThemeUndefined() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_UNDEFINED);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("false", getPrefersColorSchemeDark());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLightThemeTrue() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_TRUE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("false", getPrefersColorSchemeDark());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=WebViewForceDarkModeMatchTheme",
            "enable-features=WebViewDarkModeMatchTheme"})
    public void
    testLightThemeFalseWithMatchThemeDisabled() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
        assertFalse(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.
    Add({"enable-features=WebViewForceDarkModeMatchTheme,WebViewDarkModeMatchTheme"})
    public void testLightThemeFalse() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
        assertTrue(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewDarkModeMatchTheme"})
    public void testConfigurationChanged() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_TRUE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("false", getPrefersColorSchemeDark());
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        Configuration newConfig = new Configuration();
        newConfig.uiMode = Configuration.UI_MODE_NIGHT_YES;
        TestThreadUtils.runOnUiThreadBlocking(() -> mAwContents.onConfigurationChanged(newConfig));
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
    }

    private void loadUrlSync(String url) throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        done.waitForCallback(callCount);
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private String getPrefersColorSchemeDark() throws Throwable {
        return executeJavaScriptAndWaitForResult(
                "window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches");
    }

    private boolean isForceDarkening() throws Throwable {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mAwContents.getSettings().isDarkMode();
            }
        });
    }
}
