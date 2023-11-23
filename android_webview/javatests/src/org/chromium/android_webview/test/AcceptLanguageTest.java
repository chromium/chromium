// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.LocaleList;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Locale;
import java.util.regex.Pattern;

/** Tests for Accept Language implementation. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AcceptLanguageTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    public AcceptLanguageTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
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

    private boolean isSingleLocale(String lang, String country) {
        String languageTag = String.format("%s-%s", lang, country);
        // In N+, multiple locales can be set.
        return languageTag.equals(LocaleList.getDefault().toLanguageTags());
    }

    private void setSingleLocale(String lang, String country) {
        LocaleList.setDefault(new LocaleList(new Locale(lang, country)));
    }

    private void setLocaleForTesting(String lang, String country) {
        if (!isSingleLocale(lang, country)) {
            setSingleLocale(lang, country);
            mAwContents.updateDefaultLocale();
        }
    }

    /** Verify that the Accept Language string is correct. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAcceptLanguage() throws Throwable {
        setLocaleForTesting("en", "US");

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        // This should yield a lightly formatted page with the contents of the Accept-Language
        // header, e.g. "en-US" or "de-DE,en-US;q=0.8", as the only text content.
        String url = mTestServer.getURL("/echoheader?Accept-Language");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend the base language from language-region pair.
        String rawAcceptLanguages =
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        Assert.assertTrue(
                "Accept-Language header should contain at least 1 q-value",
                rawAcceptLanguages.contains(";q="));
        String[] acceptLanguages = getAcceptLanguages(rawAcceptLanguages);
        Assert.assertArrayEquals(new String[] {"en-US", "en"}, acceptLanguages);

        // Our accept language list in user agent is different from navigator.languages, which is
        // fine.
        String[] acceptLanguagesJs =
                getAcceptLanguages(
                        JSUtils.executeJavaScriptAndWaitForResult(
                                InstrumentationRegistry.getInstrumentation(),
                                mAwContents,
                                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                                "navigator.languages.join(',')"));
        Assert.assertArrayEquals(new String[] {"en-US"}, acceptLanguagesJs);

        // Test locale change at run time
        setLocaleForTesting("de", "DE");

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        acceptLanguages =
                getAcceptLanguages(
                        mActivityTestRule.getJavaScriptResultBodyTextContent(
                                mAwContents, mContentsClient));
        // Note that we extend the base language from language-region pair, and we put en-US and en
        // at the end.
        Assert.assertArrayEquals(new String[] {"de-DE", "de", "en-US", "en"}, acceptLanguages);
    }

    /**
     * Verify that the Accept Languages string is correct.
     * When default locales do not contain "en-US" or "en-us",
     * "en-US" should be added with lowest priority.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAcceptLanguagesWithenUS() throws Throwable {
        LocaleList.setDefault(new LocaleList(new Locale("ko", "KR")));
        mAwContents.updateDefaultLocale();

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);

        // This should yield a lightly formatted page with the contents of the Accept-Language
        // header, e.g. "en-US,en" or "de-DE,de,en-US,en;q=0.8", as the only text content.
        String url = mTestServer.getURL("/echoheader?Accept-Language");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend accept languages.
        Assert.assertArrayEquals(
                new String[] {"ko-KR", "ko", "en-US", "en"},
                getAcceptLanguages(
                        mActivityTestRule.getJavaScriptResultBodyTextContent(
                                mAwContents, mContentsClient)));

        // Our accept language list in user agent is different from navigator.languages, which is
        // fine.
        String[] acceptLanguagesJs =
                getAcceptLanguages(
                        JSUtils.executeJavaScriptAndWaitForResult(
                                InstrumentationRegistry.getInstrumentation(),
                                mAwContents,
                                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                                "navigator.languages.join(',')"));
        Assert.assertArrayEquals(new String[] {"ko-KR", "en-US"}, acceptLanguagesJs);

        // Test locales that contain "en-US" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("en", "US")));
        mAwContents.updateDefaultLocale();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Note that we extend the base language from language-region pair.
        // Also, we put en-US at the lowest priority.
        Assert.assertArrayEquals(
                new String[] {"de-DE", "de", "en-US", "en"},
                getAcceptLanguages(
                        mActivityTestRule.getJavaScriptResultBodyTextContent(
                                mAwContents, mContentsClient)));

        // Test locales that contain "en-us" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("en", "us")));
        mAwContents.updateDefaultLocale();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        Assert.assertArrayEquals(
                new String[] {"de-DE", "de", "en-US", "en"},
                getAcceptLanguages(
                        mActivityTestRule.getJavaScriptResultBodyTextContent(
                                mAwContents, mContentsClient)));

        // Test locales that do not contain "en-us" or "en-US" change at run time
        LocaleList.setDefault(new LocaleList(new Locale("de", "DE"), new Locale("ja", "JP")));
        mAwContents.updateDefaultLocale();

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        Assert.assertArrayEquals(
                new String[] {"de-DE", "de", "ja-JP", "ja", "en-US", "en"},
                getAcceptLanguages(
                        mActivityTestRule.getJavaScriptResultBodyTextContent(
                                mAwContents, mContentsClient)));
    }
}
