// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.LocaleList;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Tests for Accept Language implementation.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AcceptLanguageTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mAwContents = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                              .getAwContents();

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private static final Pattern COMMA_AND_OPTIONAL_Q_VALUE =
            Pattern.compile("(?:;q=[^,]+)?(?:,|$)");

    /**
     * Extract the languages from the Accept-Language header.
     *
     * The Accept-Language header can have more than one language along with optional quality
     * factors for each, e.g.
     *
     *  "de-DE,en-US;q=0.8,en-UK;q=0.5"
     *
     * This function extracts only the language strings from the Accept-Language header, so
     * the example above would yield the following:
     *
     *  ["de-DE", "en-US", "en-UK"]
     *
     * @param raw String containing the raw Accept-Language header
     * @return A list of languages as Strings.
     */
    private String[] getAcceptLanguages(String raw) {
        return COMMA_AND_OPTIONAL_Q_VALUE.split(mActivityTestRule.maybeStripDoubleQuotes(raw));
    }

    private boolean isEnUsLocale() {
        return "en-US".equals(Locale.getDefault().toLanguageTag());
    }

    /**
     * Verify that the Accept Language string is correct.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAcceptLanguage() throws Throwable {
        // Make sure that the current locale is en-US.
        if (!isEnUsLocale()) {
            Locale.setDefault(new Locale("en", "US"));
            AwContents.updateDefaultLocale();
            mAwContents.getSettings().updateAcceptLanguages();
        }
        Assert.assertTrue(isEnUsLocale());

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        // This should yield a lightly formatted page with the contents of the Accept-Language
        // header, e.g. "en-US" or "de-DE,en-US;q=0.8", as the only text content.
        String url = mTestServer.getURL("/echoheader?Accept-Language");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend the base language from language-region pair.
        String[] acceptLanguages = getAcceptLanguages(
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient));
        Assert.assertArrayEquals(new String[] {"en-US", "en"}, acceptLanguages);

        // Our accept language list in user agent is different from navigator.languages, which is
        // fine.
        String[] acceptLanguagesJs = getAcceptLanguages(JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(), mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "navigator.languages.join(',')"));
        Assert.assertArrayEquals(new String[] {"en-US"}, acceptLanguagesJs);

        // Test locale change at run time
        Locale.setDefault(new Locale("de", "DE"));
        AwContents.updateDefaultLocale();
        mAwContents.getSettings().updateAcceptLanguages();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        acceptLanguages = getAcceptLanguages(
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient));
        // Note that we extend the base language from language-region pair.
        Assert.assertArrayEquals(new String[] {"de-DE", "de", "en-US", "en"}, acceptLanguages);
    }

    /**
     * Verify that the Accept Languages string is correct.
     * When default locales do not contain "en-US" or "en-us",
     * "en-US" should be added with lowest priority.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @SuppressLint("NewApi")
    @Feature({"AndroidWebView"})
    public void testAcceptLanguagesWithenUS() throws Throwable {
        LocaleList.setDefault(new LocaleList(new Locale("ko", "KR")));
        AwContents.updateDefaultLocale();
        mAwContents.getSettings().updateAcceptLanguages();

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        // This should yield a lightly formatted page with the contents of the Accept-Language
        // header, e.g. "en-US,en" or "de-DE,de,en-US,en;q=0.8", as the only text content.
        String url = mTestServer.getURL("/echoheader?Accept-Language");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend accept languages.
        Assert.assertArrayEquals(new String[] {"ko-KR", "ko", "en-US", "en"},
                getAcceptLanguages(mActivityTestRule.getJavaScriptResultBodyTextContent(
                        mAwContents, mContentsClient)));

        // Our accept language list in user agent is different from navigator.languages, which is
        // fine.
        String[] acceptLanguagesJs = getAcceptLanguages(JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(), mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "navigator.languages.join(',')"));
        Assert.assertArrayEquals(new String[] {"ko-KR", "en-US"}, acceptLanguagesJs);

        // Test locales that contain "en-US" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("en", "US")));
        AwContents.updateDefaultLocale();
        mAwContents.getSettings().updateAcceptLanguages();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend the base language from language-region pair.
        // Also, we put en-US at the lowest priority.
        Assert.assertArrayEquals(new String[] {"de-DE", "de", "en-US", "en"},
                getAcceptLanguages(mActivityTestRule.getJavaScriptResultBodyTextContent(
                        mAwContents, mContentsClient)));

        // Test locales that contain "en-us" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("en", "us")));
        AwContents.updateDefaultLocale();
        mAwContents.getSettings().updateAcceptLanguages();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        Assert.assertArrayEquals(new String[] {"de-DE", "de", "en-US", "en"},
                getAcceptLanguages(mActivityTestRule.getJavaScriptResultBodyTextContent(
                        mAwContents, mContentsClient)));

        // Test locales that do not contain "en-us" or "en-US" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("ja", "JP")));
        AwContents.updateDefaultLocale();
        mAwContents.getSettings().updateAcceptLanguages();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        Assert.assertArrayEquals(new String[] {"de-DE", "de", "ja-JP", "ja", "en-US", "en"},
                getAcceptLanguages(mActivityTestRule.getJavaScriptResultBodyTextContent(
                        mAwContents, mContentsClient)));
    }
}
