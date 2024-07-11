// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.res.Configuration;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.DarkModeHelper;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/** The integration test for the dark mode. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public class AwDarkModeTest extends AwParameterizedTest {
    private static final String FILE = "/main.html";
    private static final String DATA =
            "<html><head><meta name=\"color-scheme\" content=\"dark light\"></head>"
                    + "<body>DarkMode</body></html>";

    @Rule public AwActivityTestRule mRule;

    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private CallbackHelper mCallbackHelper = new CallbackHelper();
    private AwContents mAwContents;

    public AwDarkModeTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
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
    @CommandLineFlags.Add({"disable-features=WebViewForceDarkModeMatchTheme"})
    public void testLightThemeFalseWithMatchThemeDisabled() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
        assertFalse(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewForceDarkModeMatchTheme"})
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
    public void testConfigurationChanged() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_TRUE);
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("false", getPrefersColorSchemeDark());
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        Configuration newConfig = new Configuration();
        newConfig.uiMode = Configuration.UI_MODE_NIGHT_YES;
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.onConfigurationChanged(newConfig));
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAlgorithmicDarkeningAllowedOnAndroidT() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        AwDarkMode.enableSimplifiedDarkMode();

        // Check setForceDarkMode has noops, otherwise ForceDarkening will be turned off.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);
        mAwContents.getSettings().setAlgorithmicDarkeningAllowed(true);
        // Set force dark mode again to check no ordering issue.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
        assertTrue(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAlgorithmicDarkeningAllowedWithLightThemeOnAndroidT() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_TRUE);
        AwDarkMode.enableSimplifiedDarkMode();

        // Check setForceDarkMode has noops, otherwise ForceDarkening will be turned off.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);
        mAwContents.getSettings().setAlgorithmicDarkeningAllowed(true);
        // Set force dark mode again to check no ordering issue.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        // Verify that prefers-color-scheme matches the theme.
        assertEquals("false", getPrefersColorSchemeDark());
        // Algorithmic darkening isn't enabled because app's light theme.
        assertFalse(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAlgorithmicDarkeningDisallowedByDefaultOnAndroidT() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        AwDarkMode.enableSimplifiedDarkMode();

        // Check setForceDarkMode has noops, otherwise ForceDarkening will be turned on.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_ON);

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        assertEquals("true", getPrefersColorSchemeDark());
        assertFalse(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPrefersColorSchemeDarkOnAndroidT() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_FALSE);
        AwDarkMode.enableSimplifiedDarkMode();

        // Check setForceDarkMode has noops, otherwise, prefers-color-scheme will be set to light.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        // Verify prefers-color-scheme matches isLightTheme.
        assertEquals("true", getPrefersColorSchemeDark());
        assertFalse(isForceDarkening());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPrefersColorSchemeLightOnAndroidT() throws Throwable {
        DarkModeHelper.setsLightThemeForTesting(DarkModeHelper.LightTheme.LIGHT_THEME_TRUE);
        AwDarkMode.enableSimplifiedDarkMode();

        // Check setForceDarkMode has noops, otherwise, prefers-color-scheme will be set to dark.
        mAwContents.getSettings().setForceDarkMode(AwSettings.FORCE_DARK_OFF);

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        // Verify prefers-color-scheme matches isLightTheme.
        assertEquals("false", getPrefersColorSchemeDark());
        assertFalse(isForceDarkening());
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
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mAwContents.getSettings().isForceDarkApplied();
                    }
                });
    }
}
