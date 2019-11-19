// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.speech.RecognizerResultsIntent;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CollectionUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.test.CommandLineInitRule;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for IntentHandler.
 * TODO(nileshagrawal): Add tests for onNewIntent.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class IntentHandlerTest {
    @Rule
    public final RuleChain mChain = RuleChain.outerRule(new CommandLineInitRule(null))
                                            .around(new ChromeBrowserTestRule())
                                            .around(new UiThreadTestRule());

    private static final String VOICE_SEARCH_QUERY = "VOICE_QUERY";
    private static final String VOICE_SEARCH_QUERY_URL = "http://www.google.com/?q=VOICE_QUERY";

    private static final String VOICE_URL_QUERY = "www.google.com";
    private static final String VOICE_URL_QUERY_URL = "INVALID_URLZ";

    private static final String[] ACCEPTED_NON_HTTP_AND_HTTPS_URLS = {"chrome://newtab",
            "file://foo.txt", "ftp://www.foo.com", "", "://javascript:80/hello",
            "ftp@https://confusing:@something.example:5/goat?sayit", "://www.google.com/",
            "chrome-search://food",
            "java-scr\nipt://alert", // - is significant
            "java.scr\nipt://alert", // . is significant
            "java+scr\nipt://alert", // + is significant
            "http ://time", "iris.beep:app"};

    private static final String[] REJECTED_INTENT_URLS = {"javascript://", " javascript:alert(1) ",
            "jar:http://www.example.com/jarfile.jar!/",
            "jar:http://www.example.com/jarfile.jar!/mypackage/myclass.class",
            "  \tjava\nscript\n:alert(1)  ", "javascript://window.opener",
            "   javascript:fun@somethings.com/yeah", " j\na\nr\t:f:oobarz ",
            "jar://http://@foo.com/test.html", "  jar://https://@foo.com/test.html",
            "javascript:http//bar.net:javascript/yes.no", " javascript:://window.open(1)",
            " java script:alert(1)", "~~~javascript://alert"};

    private static final String[] VALID_HTTP_AND_HTTPS_URLS = {"http://www.google.com",
            "http://movies.nytimes.com/movie/review?"
                    + "res=9405EFDB1E3BE23BBC4153DFB7678382659EDE&partner=Rotten Tomatoes",
            "https://www.gmail.com", "http://www.example.com/\u00FCmlat.html&q=name",
            "http://www.example.com/quotation_\"", "http://www.example.com/lessthansymbol_<",
            "http://www.example.com/greaterthansymbol_>", "http://www.example.com/poundcharacter_#",
            "http://www.example.com/percentcharacter_%", "http://www.example.com/leftcurlybrace_{",
            "http://www.example.com/rightcurlybrace_}", "http://www.example.com/verticalpipe_|",
            "http://www.example.com/backslash_\\", "http://www.example.com/caret_^",
            "http://www.example.com/tilde_~", "http://www.example.com/leftsquarebracket_[",
            "http://www.example.com/rightsquarebracket_]", "http://www.example.com/graveaccent_`",
            "www.example.com", "www.google.com", "www.bing.com", "notreallyaurl",
            "https:awesome@google.com/haha.gif", "//www.google.com"};

    private static final String[] REJECTED_GOOGLECHROME_URLS = {
            IntentHandler.GOOGLECHROME_SCHEME + "://reddit.com",
            IntentHandler.GOOGLECHROME_SCHEME + "://navigate?reddit.com",
            IntentHandler.GOOGLECHROME_SCHEME + "://navigate?urlreddit.com",
            IntentHandler.GOOGLECHROME_SCHEME
                    + "://navigate?url=content://com.android.chrome.FileProvider",
    };

    private static final String[][] INTENT_URLS_AND_TYPES_FOR_MHTML = {
            {"file://foo.mhtml", ""}, {"file://foo.mht", ""}, {"file://foo!.mht", ""},
            {"file://foo!.mhtml", ""}, {"file://foo.mhtml", "application/octet-stream"},
            {"file://foo.mht", "application/octet-stream"}, {"file://foo", "multipart/related"},
            {"file://foo", "message/rfc822"}, {"content://example.com/1", "multipart/related"},
            {"content://example.com/1", "message/rfc822"},
    };

    private static final String[][] INTENT_URLS_AND_TYPES_NOT_FOR_MHTML = {
            {"http://www.example.com", ""}, {"ftp://www.example.com", ""}, {"file://foo", ""},
            {"file://foo", "application/octet-stream"}, {"file://foo.txt", ""},
            {"file://foo.mhtml", "text/html"}, {"content://example.com/1", ""},
            {"content://example.com/1", "text/html"},
    };

    private static final String GOOGLE_URL = "https://www.google.com";

    private IntentHandler mIntentHandler;
    private Intent mIntent;

    private void processUrls(String[] urls, boolean isValid) {
        List<String> failedTests = new ArrayList<String>();

        for (String url : urls) {
            mIntent.setData(Uri.parse(url));
            if (IntentHandler.intentHasValidUrl(mIntent) != isValid) {
                failedTests.add(url);
            }
        }
        Assert.assertTrue(failedTests.toString(), failedTests.isEmpty());
    }

    private void checkIntentForMhtmlFileOrContent(String[][] urlsAndTypes, boolean isValid) {
        List<String> failedTests = new ArrayList<>();

        for (String[] urlAndType : urlsAndTypes) {
            Uri url = Uri.parse(urlAndType[0]);
            String type = urlAndType[1];
            if (type.equals("")) {
                mIntent.setData(url);
            } else {
                mIntent.setDataAndType(url, type);
            }
            if (IntentHandler.isIntentForMhtmlFileOrContent(mIntent) != isValid) {
                failedTests.add(url.toString() + "," + type);
            }
        }
        Assert.assertTrue(
                "Expect " + isValid + " Actual " + !isValid + ": " + failedTests.toString(),
                failedTests.isEmpty());
    }

    @Before
    public void setUp() {
        IntentHandler.setTestIntentsEnabled(false);
        mIntentHandler = new IntentHandler(null, null);
        mIntent = new Intent();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAcceptedUrls() {
        processUrls(ACCEPTED_NON_HTTP_AND_HTTPS_URLS, true);
        processUrls(VALID_HTTP_AND_HTTPS_URLS, true);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedUrls() {
        processUrls(REJECTED_INTENT_URLS, false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAcceptedGoogleChromeSchemeNavigateUrls() {
        String[] expectedAccepts = new String[VALID_HTTP_AND_HTTPS_URLS.length];
        for (int i = 0; i < VALID_HTTP_AND_HTTPS_URLS.length; ++i) {
            expectedAccepts[i] =
                    IntentHandler.GOOGLECHROME_NAVIGATE_PREFIX + VALID_HTTP_AND_HTTPS_URLS[i];
        }
        processUrls(expectedAccepts, true);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedGoogleChromeSchemeNavigateUrls() {
        // Test all of the rejected URLs after prepending googlechrome://navigate?url.
        String[] expectedRejections = new String[REJECTED_INTENT_URLS.length];
        for (int i = 0; i < REJECTED_INTENT_URLS.length; ++i) {
            expectedRejections[i] =
                    IntentHandler.GOOGLECHROME_NAVIGATE_PREFIX + REJECTED_INTENT_URLS[i];
        }
        processUrls(expectedRejections, false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRejectedGoogleChromeSchemeUrls() {
        List<String> failedTests = new ArrayList<String>();

        for (String url : REJECTED_GOOGLECHROME_URLS) {
            mIntent.setData(Uri.parse(url));
            if (IntentHandler.getUrlFromIntent(mIntent) != null) {
                failedTests.add(url);
            }
        }
        Assert.assertTrue(failedTests.toString(), failedTests.isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testNullUrlIntent() {
        mIntent.setData(null);
        Assert.assertTrue(
                "Intent with null data should be valid", IntentHandler.intentHasValidUrl(mIntent));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validVoiceQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                CollectionUtil.newArrayList(VOICE_SEARCH_QUERY));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                CollectionUtil.newArrayList(VOICE_SEARCH_QUERY_URL));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        Assert.assertEquals(VOICE_SEARCH_QUERY_URL, query);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validUrlQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                CollectionUtil.newArrayList(VOICE_URL_QUERY));
        intent.putStringArrayListExtra(RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                CollectionUtil.newArrayList(VOICE_URL_QUERY_URL));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        Assert.assertTrue(String.format("Expected qualified URL: %s, to start "
                                          + "with http://www.google.com",
                                  query),
                query.indexOf("http://www.google.com") == 0);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraReferrer() {
        // Check that EXTRA_REFERRER is not accepted with a random URL.
        Intent foreignIntent = new Intent(Intent.ACTION_VIEW);
        foreignIntent.putExtra(Intent.EXTRA_REFERRER, GOOGLE_URL);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(foreignIntent));

        // Check that EXTRA_REFERRER with android-app URL works.
        String appUrl = "android-app://com.application/http/www.application.com";
        Intent appIntent = new Intent(Intent.ACTION_VIEW);
        appIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(appUrl));
        Assert.assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(appIntent));

        // Ditto, with EXTRA_REFERRER_NAME.
        Intent nameIntent = new Intent(Intent.ACTION_VIEW);
        nameIntent.putExtra(Intent.EXTRA_REFERRER_NAME, appUrl);
        Assert.assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(nameIntent));

        // Check that EXTRA_REFERRER with an empty host android-app URL doesn't work.
        appUrl = "android-app:///www.application.com";
        appIntent = new Intent(Intent.ACTION_VIEW);
        appIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(appUrl));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(appIntent));

        // Ditto, with EXTRA_REFERRER_NAME.
        nameIntent = new Intent(Intent.ACTION_VIEW);
        nameIntent.putExtra(Intent.EXTRA_REFERRER_NAME, appUrl);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(nameIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersInclReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("X-custom-header", "X-custom-value");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals("X-custom-header: X-custom-value",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersInclRefererMultiple() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("X-custom-header", "X-custom-value");
        bundle.putString("X-custom-header-2", "X-custom-value-2");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals("X-custom-header-2: X-custom-value-2\nX-custom-header: X-custom-value",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersOnlyReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersAndExtraReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        headersIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(validReferer));
        Assert.assertEquals(
                validReferer, IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_extraHeadersValidReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", validReferer);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals(
                validReferer, IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAddTimestampToIntent() {
        Intent intent = new Intent();
        Assert.assertEquals(-1, IntentHandler.getTimestampFromIntent(intent));
        // Check both before and after to make sure that the returned value is
        // really from {@link SystemClock#elapsedRealtime()}.
        long before = SystemClock.elapsedRealtime();
        IntentHandler.addTimestampToIntent(intent);
        long after = SystemClock.elapsedRealtime();
        Assert.assertTrue("Time should be increasing",
                before <= IntentHandler.getTimestampFromIntent(intent));
        Assert.assertTrue(
                "Time should be increasing", IntentHandler.getTimestampFromIntent(intent) <= after);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGeneratedReferrer() {
        Context context = InstrumentationRegistry.getTargetContext();
        String packageName = context.getPackageName();
        String referrer = IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        Assert.assertEquals("android-app://" + packageName, referrer);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRemoveChromeCustomHeaderFromExtraIntentHeaders() {
        Bundle bundle = new Bundle();
        bundle.putString("X-Chrome-intent-type", "X-custom-value");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testLogHeaders() {
        Bundle bundle = new Bundle();
        bundle.putString("Content-Length", "1234");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);

        IntentHandler.getExtraHeadersFromIntent(headersIntent);
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting("Android.IntentHeaders"));

        IntentHandler.getExtraHeadersFromIntent(headersIntent, true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting("Android.IntentHeaders"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMaybeAddAdditionalExtraHeaders() {
        String contentUrl = "content://com.example.org/document/1";
        Intent intent = new Intent();

        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(null, null, null));
        // Null URL.
        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(intent, null, null));
        // Null intent.
        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(null, contentUrl, null));
        // Null type.
        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, null));
        // Empty type.
        intent.setType("");
        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, null));

        // Type not used by MHTML.
        intent.setType("text/plain");
        Assert.assertNull(IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, null));

        // MHTML type with no extra headers.
        intent.setType("multipart/related");
        Assert.assertEquals("X-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, null));

        // MHTML type with extra headers.
        intent.setType("multipart/related");
        Assert.assertEquals("Foo: bar\nX-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, "Foo: bar"));

        // Different MHTML type.
        intent.setType("message/rfc822");
        Assert.assertEquals("X-Chrome-intent-type: message/rfc822",
                IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, null));

        // Different MHTML type with extra headers.
        intent.setType("message/rfc822");
        Assert.assertEquals("Foo: bar\nX-Chrome-intent-type: message/rfc822",
                IntentHandler.maybeAddAdditionalExtraHeaders(intent, contentUrl, "Foo: bar"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIsIntentForMhtmlFileOrContent() {
        checkIntentForMhtmlFileOrContent(INTENT_URLS_AND_TYPES_FOR_MHTML, true);
        checkIntentForMhtmlFileOrContent(INTENT_URLS_AND_TYPES_NOT_FOR_MHTML, false);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCreateTrustedOpenNewTabIntent() {
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(context, true);

        Assert.assertEquals(intent.getAction(), Intent.ACTION_VIEW);
        Assert.assertEquals(intent.getData(), Uri.parse(UrlConstants.NTP_URL));
        Assert.assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(intent));
        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));

        intent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        Assert.assertFalse(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true));
    }
}
