// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler.RequestMetadata;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.Referrer;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for IntentHandler.
 * TODO(nileshagrawal): Add tests for onNewIntent.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IntentHandlerUnitTest {
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
            {"file://foo.mhtml", ""},
            {"file://foo.mht", ""},
            {"file://foo!.mht", ""},
            {"file://foo!.mhtml", ""},
            {"file://foo.mhtml", "application/octet-stream"},
            {"file://foo.mht", "application/octet-stream"},
            {"file://foo", "multipart/related"},
            {"file://foo", "message/rfc822"},
            {"content://example.com/1", "multipart/related"},
            {"content://example.com/1", "message/rfc822"},
    };

    private static final String[][] INTENT_URLS_AND_TYPES_NOT_FOR_MHTML = {
            {"http://www.example.com", ""},
            {"ftp://www.example.com", ""},
            {"file://foo", ""},
            {"file://foo", "application/octet-stream"},
            {"file://foo.txt", ""},
            {"file://foo.mhtml", "text/html"},
            {"content://example.com/1", ""},
            {"content://example.com/1", "text/html"},
    };

    private static final Object[][] SHARE_INTENT_CASES = {
            {"Check this out! https://example.com/foo#bar", "https://example.com/foo#bar", 1},
            {"This http://www.example.com URL is bussin fr fr.\nhttp://www.example.com/foo",
                    "http://www.example.com/foo", 2},
            {"http://one.com https://two.com http://three.com http://four.com", "https://two.com",
                    4},
            {"https://example.com", "https://example.com", 1},
            {"https://example.com Sent from my iPhone.", "https://example.com", 1},
            {"https://example.com\nSent from my iPhone.", "https://example.com", 1},
            {"~(_8^(|)", null, 0},
            {"", null, 0},
            {null, null, 0},
    };

    private static final String GOOGLE_URL = "https://www.google.com";

    private IntentHandler mIntentHandler;
    private Intent mIntent;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    public IntentHandler.IntentHandlerDelegate mDelegate;
    @Captor
    ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

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
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        IntentHandler.setTestIntentsEnabled(false);
        mIntentHandler = new IntentHandler(null, mDelegate);
        mIntent = new Intent();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.OPAQUE_ORIGIN_FOR_INCOMING_INTENTS)
    public void testNewIntentInitiator() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        InOrder inOrder = Mockito.inOrder(mDelegate);

        mIntentHandler.onNewIntent(intent);
        inOrder.verify(mDelegate).processUrlViewIntent(
                mLoadUrlParamsCaptor.capture(), anyInt(), any(), anyInt(), eq(intent));
        Assert.assertTrue(mLoadUrlParamsCaptor.getValue().getInitiatorOrigin().isOpaque());

        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        mIntentHandler.onNewIntent(intent);
        inOrder.verify(mDelegate).processUrlViewIntent(
                mLoadUrlParamsCaptor.capture(), anyInt(), any(), anyInt(), eq(intent));
        Assert.assertNull(mLoadUrlParamsCaptor.getValue().getInitiatorOrigin());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.OPAQUE_ORIGIN_FOR_INCOMING_INTENTS)
    public void testNewIntentInitiatorFromRenderer() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        InOrder inOrder = Mockito.inOrder(mDelegate);

        mIntentHandler.onNewIntent(intent);
        inOrder.verify(mDelegate).processUrlViewIntent(
                mLoadUrlParamsCaptor.capture(), anyInt(), any(), anyInt(), eq(intent));
        Assert.assertNull(mLoadUrlParamsCaptor.getValue().getInitiatorOrigin());

        RequestMetadata metadata = new RequestMetadata(true, true);
        IntentWithRequestMetadataHandler.getInstance().onNewIntentWithRequestMetadata(
                intent, metadata);

        mIntentHandler.onNewIntent(intent);
        inOrder.verify(mDelegate).processUrlViewIntent(
                mLoadUrlParamsCaptor.capture(), anyInt(), any(), anyInt(), eq(intent));
        Assert.assertTrue(mLoadUrlParamsCaptor.getValue().getInitiatorOrigin().isOpaque());
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
    @UiThreadTest
    @Feature({"Android-Appbase"})
    public void testUrlFromIntent_WebappUrl() {
        Intent webappLauncherActivityIntent =
                WebappTestHelper.createMinimalWebappIntent("id", GOOGLE_URL);
        WebappLauncherActivity.LaunchData launchData = new WebappLauncherActivity.LaunchData("id",
                GOOGLE_URL, null /* webApkPackageName */, false /* isSplashProvidedByWebApk */);
        mIntent = WebappLauncherActivity.createIntentToLaunchForWebapp(
                webappLauncherActivityIntent, launchData, 0);
        Assert.assertEquals(GOOGLE_URL, IntentHandler.getUrlFromIntent(mIntent));
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
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraReferrer() {
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
    public void testReferrerUrl_extraHeadersInclReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Accept", "application/xhtml+xml");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals("Accept: application/xhtml+xml",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersInclRefererMultiple() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Accept", "application/xhtml+xml");
        bundle.putString("Content-Language", "de-DE, en-CA");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals("Content-Language: de-DE, en-CA\nAccept: application/xhtml+xml",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersOnlyReferer() {
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
    public void testReferrerUrl_extraHeadersAndExtraReferrer() {
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
    public void testReferrerUrl_extraHeadersValidReferrer() {
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
    public void testExtraHeadersVerifiedOrigin() throws Exception {
        // Check that non-allowlisted headers from extras are passed
        // when origin is verified.
        Context context = ApplicationProvider.getApplicationContext();
        Intent headersIntent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                context, "https://www.google.com/");

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        headersIntent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(headersIntent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ChromeOriginVerifier.addVerificationOverride("app1",
                                Origin.create(headersIntent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        String extraHeaders = IntentHandler.getExtraHeadersFromIntent(headersIntent);
        assertTrue(extraHeaders.contains("bearer-token: Some token"));
        assertTrue(extraHeaders.contains("redirect-url: https://www.google.com"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeOriginVerifier.clearCachedVerificationsForTesting());
    }

    @Test
    @SmallTest
    public void testExtraHeadersNonVerifiedOrigin() throws Exception {
        // Check that non-allowlisted headers from extras are passed
        // when origin is verified.
        Context context = ApplicationProvider.getApplicationContext();
        Intent headersIntent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                context, "https://www.google.com/");

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        headersIntent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(headersIntent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ChromeOriginVerifier.addVerificationOverride("app2",
                                Origin.create(headersIntent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        String extraHeaders = IntentHandler.getExtraHeadersFromIntent(headersIntent);
        assertNull(extraHeaders);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeOriginVerifier.clearCachedVerificationsForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_customTabIntentWithSession() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                context, "https://www.google.com/");
        Assert.assertTrue(CustomTabsConnection.getInstance().newSession(
                CustomTabsSessionToken.getSessionTokenFromIntent(intent)));
        Assert.assertEquals("android-app://org.chromium.chrome.tests",
                IntentHandler.getReferrerUrlIncludingExtraHeaders(intent));
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
        Context context = ApplicationProvider.getApplicationContext();
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
    public void testStripNonCorsSafelistedCustomHeader() {
        Bundle bundle = new Bundle();
        bundle.putString("X-Some-Header", "1");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testIgnoreHeaderNewLineInValue() {
        Bundle bundle = new Bundle();
        bundle.putString("sec-ch-ua-full", "\nCookie: secret=cookie");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testmaybeAddAdditionalContentHeaders() {
        String contentUrl = "content://com.example.org/document/1";
        Intent intent = new Intent();

        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(null, null, null));
        // Null URL.
        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(intent, null, null));
        // Null intent.
        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(null, contentUrl, null));
        // Null type.
        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));
        // Empty type.
        intent.setType("");
        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // Type not used by MHTML.
        intent.setType("text/plain");
        Assert.assertNull(IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // MHTML type with no extra headers.
        intent.setType("multipart/related");
        Assert.assertEquals("X-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // MHTML type with extra headers.
        intent.setType("multipart/related");
        Assert.assertEquals("Foo: bar\nX-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, "Foo: bar"));

        // Different MHTML type.
        intent.setType("message/rfc822");
        Assert.assertEquals("X-Chrome-intent-type: message/rfc822",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // Different MHTML type with extra headers.
        intent.setType("message/rfc822");
        Assert.assertEquals("Foo: bar\nX-Chrome-intent-type: message/rfc822",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, "Foo: bar"));
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
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(context, true);

        Assert.assertEquals(intent.getAction(), Intent.ACTION_VIEW);
        Assert.assertEquals(intent.getData(), Uri.parse(UrlConstants.NTP_URL));
        Assert.assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(intent));
        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));

        intent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        assertFalse(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true));
    }

    /**
     * Test that IntentHandler#shouldIgnoreIntent() returns false for Webapp launch intents.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIntentWebapp() {
        Intent webappLauncherActivityIntent =
                WebappTestHelper.createMinimalWebappIntent("id", GOOGLE_URL);
        WebappLauncherActivity.LaunchData launchData = new WebappLauncherActivity.LaunchData("id",
                GOOGLE_URL, null /* webApkPackageName */, false /* isSplashProvidedByWebApk */);
        Intent intent = WebappLauncherActivity.createIntentToLaunchForWebapp(
                webappLauncherActivityIntent, launchData, 0);

        assertFalse(mIntentHandler.shouldIgnoreIntent(intent));
    }

    /**
     * Test that IntentHandler#shouldIgnoreIntent() returns true for Incognito non-Custom Tab
     * Intents.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIncognitoIntent() {
        Intent intent = new Intent(GOOGLE_URL);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        assertTrue(mIntentHandler.shouldIgnoreIntent(intent));
    }

    /**
     * Test that IntentHandler#shouldIgnoreIntent() returns false for Incognito non-Custom Tab
     * Intents if they come from Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIncognitoIntent_trusted() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(context, true);
        assertFalse(mIntentHandler.shouldIgnoreIntent(intent));
    }

    /**
     * Test that IntentHandler#shouldIgnoreIntent() returns false for Incognito Custom Tab Intents.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIncognitoIntent_customTab() {
        Intent intent = new Intent(GOOGLE_URL);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        assertFalse(mIntentHandler.shouldIgnoreIntent(intent, /*isCustomTab=*/true));
    }

    @Test
    @SmallTest
    public void testIgnoreUnauthenticatedBringToFront() {
        int tabId = 1;
        Intent intent = IntentHandler.createTrustedBringTabToFrontIntent(
                tabId, IntentHandler.BringToFrontSource.ACTIVATE_TAB);
        assertEquals(tabId, IntentHandler.getBringTabToFrontId(intent));

        intent.removeExtra("trusted_application_code_extra");
        assertEquals(Tab.INVALID_TAB_ID, IntentHandler.getBringTabToFrontId(intent));
    }

    @Test
    @SmallTest
    public void testRewriteFromHistoryIntent() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("about:blank"));
        intent.setComponent(new ComponentName(
                ContextUtils.getApplicationContext(), IntentHandlerUnitTest.class));
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        intent.putExtra("key", true);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertEquals(intent, IntentHandler.rewriteFromHistoryIntent(intent));

        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY);
        Intent newIntent = IntentHandler.rewriteFromHistoryIntent(intent);

        Intent expected = new Intent(intent);
        expected.setAction(Intent.ACTION_MAIN);
        expected.removeExtra("key");
        expected.addCategory(Intent.CATEGORY_LAUNCHER);
        expected.setData(null);
        assertEquals(expected.toUri(0), newIntent.toUri(0));
    }

    @Test
    @SmallTest
    public void testGetUrlFromShareIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        Assert.assertNull(IntentHandler.getUrlFromShareIntent(intent));
        for (Object[] shareCase : SHARE_INTENT_CASES) {
            intent.putExtra(Intent.EXTRA_TEXT, (String) shareCase[0]);
            int before = RecordHistogram.getHistogramValueCountForTesting(
                    IntentHandler.SHARE_INTENT_HISTOGRAM, (int) shareCase[2]);
            Assert.assertEquals((String) shareCase[1], IntentHandler.getUrlFromShareIntent(intent));
            Assert.assertEquals("Test case: " + (String) shareCase[0], before + 1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            IntentHandler.SHARE_INTENT_HISTOGRAM, (int) shareCase[2]));
        }
    }

    @Test
    @SmallTest
    public void testNewIntentInitiatorFromNewTabUrl() {
        int tabId = 1;
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        InOrder inOrder = Mockito.inOrder(mDelegate);
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentHandler.setTabId(intent, tabId);

        // Setup the tab's LoadUrlParams.
        Referrer urlReferrer = new Referrer(GOOGLE_URL, 0);
        LoadUrlParams loadUrlParams = new LoadUrlParams(GOOGLE_URL);
        loadUrlParams.setReferrer(urlReferrer);
        AsyncTabParamsManagerSingleton.getInstance().add(
                tabId, new AsyncTabCreationParams(loadUrlParams));

        mIntentHandler.onNewIntent(intent);
        inOrder.verify(mDelegate).processUrlViewIntent(
                mLoadUrlParamsCaptor.capture(), anyInt(), any(), anyInt(), eq(intent));
        Assert.assertEquals(
                "LoadUrlParams should match.", loadUrlParams, mLoadUrlParamsCaptor.getValue());
        Assert.assertNotNull(
                "The referrer should be non-null.", mLoadUrlParamsCaptor.getValue().getReferrer());
    }
}
