// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.settings.ForceDarkBehavior;
import org.chromium.android_webview.settings.ForceDarkMode;

/** Tests dark-mode related data are correctly passed to blink. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class DarkModeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mRule;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwContents mContents;
    private AwSettings mSettings;

    public DarkModeTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContents = createAwContentsJsEnabled();
        mSettings = mContents.getSettings();
    }

    @Test
    @SmallTest
    public void testDarkModeOff() throws Throwable {
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Disable dark mode and check that prefers-color-scheme is not set to dark.
        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_OFF);
        assertNotDarkScheme(mContents);
    }

    @Test
    @SmallTest
    public void testUAOnlyDarkening() throws Throwable {
        // If WebView uses UA only darkening strategy prefer-color-scheme should always been set to
        // 'light' to avoid double darkening regardless whether web-page supports dark theme.
        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        mSettings.setForceDarkBehavior(ForceDarkBehavior.FORCE_DARK_ONLY);

        // Load web-page which does not support dark theme and
        // check prefers-color-scheme is not set to dark
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertNotDarkScheme(mContents);

        // Load web page which supports dark theme and check prefers-color-scheme is still not set
        // to dark
        final String supportsDarkScheme =
                "<html><head><meta name=\"color-scheme\" content=\"dark light\"></head>"
                        + "<body></body></html>";
        mRule.loadHtmlSync(
                mContents, mContentsClient.getOnPageFinishedHelper(), supportsDarkScheme);
        assertNotDarkScheme(mContents);
    }

    @Test
    @SmallTest
    public void testWebThemeOnlyDarkening() throws Throwable {
        // If WebView uses web theme only darkening strategy
        // prefer-color-scheme should been set to dark.
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        mSettings.setForceDarkBehavior(ForceDarkBehavior.MEDIA_QUERY_ONLY);

        assertDarkScheme(mContents);
    }

    @Test
    @SmallTest
    public void testPreferWebThemeDarkening() throws Throwable {
        // If WebView prefers web theme darkening over UA darkening prefer-color-scheme is set
        // to 'dark'
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        mSettings.setForceDarkBehavior(ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK);

        // If web page does not support dark theme prefer-color-scheme should be still be 'dark'
        assertDarkScheme(mContents);

        final String supportsDarkScheme =
                "<html><head><meta name=\"color-scheme\" content=\"dark light\"></head>"
                        + "<body></body></html>";
        mRule.loadHtmlSync(
                mContents, mContentsClient.getOnPageFinishedHelper(), supportsDarkScheme);

        // If web page supports dark theme, prefer-color-scheme is set to dark.
        assertDarkScheme(mContents);
    }

    @Test
    @SmallTest
    public void testDarkThemePerWebView() throws Throwable {
        // WebView allows to set dark mode preferences per WebView, check that their settings
        // do not interfere with each other.

        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        mSettings.setForceDarkBehavior(ForceDarkBehavior.MEDIA_QUERY_ONLY);

        AwContents otherContents = createAwContentsJsEnabled();
        AwSettings otherSettings = otherContents.getSettings();
        mRule.loadUrlSync(otherContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        otherSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        otherSettings.setForceDarkBehavior(ForceDarkBehavior.FORCE_DARK_ONLY);

        assertDarkScheme(mContents);
        assertNotDarkScheme(otherContents);
    }

    @Test
    @SmallTest
    public void testColorSchemeUpdatedInPreferWebThemeMode() throws Throwable {
        // Test that preferred-color-scheme is updated respectively in
        // prefer-web-theme-over-ua-darkening-mode, and force dark is
        // correctly applied when web theme is not available.

        mSettings.setForceDarkMode(ForceDarkMode.FORCE_DARK_ON);
        mSettings.setForceDarkBehavior(ForceDarkBehavior.PREFER_MEDIA_QUERY_OVER_FORCE_DARK);

        // Load a web-page without dark theme support and check that preferred-color-scheme is set
        // to 'dark'
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertDarkScheme(mContents);

        // Load a web-page with dark theme support in them same WebView and check that
        // preferred-color-scheme is set to dark, so media query is applied
        final String supportsDarkScheme =
                "<html><head><meta name=\"color-scheme\" content=\"dark light\"></head>"
                        + "<body></body></html>";
        mRule.loadHtmlSync(
                mContents, mContentsClient.getOnPageFinishedHelper(), supportsDarkScheme);
        assertDarkScheme(mContents);

        // Load a web-page with no dark theme support in them same WebView and check that
        // preferred-color-scheme is still 'dark'
        mRule.loadUrlSync(mContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertDarkScheme(mContents);
    }

    private boolean prefersDarkTheme(AwContents contents) throws Exception {
        final String colorSchemeSelector =
                "window.matchMedia('(prefers-color-scheme: dark)').matches";
        String result =
                mRule.executeJavaScriptAndWaitForResult(
                        contents, mContentsClient, colorSchemeSelector);

        return "true".equals(result);
    }

    private void assertNotDarkScheme(AwContents contents) throws Exception {
        Assert.assertFalse(prefersDarkTheme(contents));
    }

    private void assertDarkScheme(AwContents contents) throws Exception {
        Assert.assertTrue(prefersDarkTheme(contents));
    }

    private AwContents createAwContentsJsEnabled() {
        AwTestContainerView view = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents contents = view.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        return contents;
    }
}
