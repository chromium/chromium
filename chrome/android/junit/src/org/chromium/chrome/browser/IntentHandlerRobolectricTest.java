// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.KeyguardManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.PowerManager;
import android.provider.Browser;
import android.view.Display;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplayManager;
import org.robolectric.shadows.ShadowKeyguardManager;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;

import java.util.ArrayList;
import java.util.List;

/**
 * Robolectric tests for IntentHandler. These tests do not require use of the native library (other
 * than GURL/Origin).
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IntentHandlerRobolectricTest {
    private static final String[] ACCEPTED_NON_HTTP_AND_HTTPS_URLS = {
        "chrome://newtab",
        "file://foo.txt",
        "ftp://www.foo.com",
        "",
        "://javascript:80/hello",
        "ftp@https://confusing:@something.example:5/goat?sayit",
        "://www.google.com/",
        "chrome-search://food",
        "java-scr\nipt://alert", // - is significant
        "java.scr\nipt://alert", // . is significant
        "java+scr\nipt://alert", // + is significant
        "http ://time",
        "iris.beep:app"
    };

    private static final String[] REJECTED_INTENT_URLS = {
        "javascript://",
        " javascript:alert(1) ",
        "jar:http://www.example.com/jarfile.jar!/",
        "jar:http://www.example.com/jarfile.jar!/mypackage/myclass.class",
        "  \tjava\nscript\n:alert(1)  ",
        "javascript://window.opener",
        "   javascript:fun@somethings.com/yeah",
        " j\na\nr\t:f:oobarz ",
        "jar://http://@foo.com/test.html",
        "  jar://https://@foo.com/test.html",
        "javascript:http//bar.net:javascript/yes.no",
        " javascript:://window.open(1)",
        " java script:alert(1)",
        "~~~javascript://alert"
    };

    private static final String[] VALID_HTTP_AND_HTTPS_URLS = {
        "http://www.google.com",
        "http://movies.nytimes.com/movie/review?"
                + "res=9405EFDB1E3BE23BBC4153DFB7678382659EDE&partner=Rotten Tomatoes",
        "https://www.gmail.com",
        "http://www.example.com/\u00FCmlat.html&q=name",
        "http://www.example.com/quotation_\"",
        "http://www.example.com/lessthansymbol_<",
        "http://www.example.com/greaterthansymbol_>",
        "http://www.example.com/poundcharacter_#",
        "http://www.example.com/percentcharacter_%",
        "http://www.example.com/leftcurlybrace_{",
        "http://www.example.com/rightcurlybrace_}",
        "http://www.example.com/verticalpipe_|",
        "http://www.example.com/backslash_\\",
        "http://www.example.com/caret_^",
        "http://www.example.com/tilde_~",
        "http://www.example.com/leftsquarebracket_[",
        "http://www.example.com/rightsquarebracket_]",
        "http://www.example.com/graveaccent_`",
        "www.example.com",
        "www.google.com",
        "www.bing.com",
        "notreallyaurl",
        "https:awesome@google.com/haha.gif",
        "//www.google.com"
    };

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
        {
            "This http://www.example.com URL is bussin fr fr.\nhttp://www.example.com/foo",
            "http://www.example.com/foo",
            2
        },
        {"http://one.com https://two.com http://three.com http://four.com", "https://two.com", 4},
        {"https://example.com", "https://example.com", 1},
        {"https://example.com Sent from my iPhone.", "https://example.com", 1},
        {"https://example.com\nSent from my iPhone.", "https://example.com", 1},
        {"~(_8^(|)", null, 0},
        {"", null, 0},
        {null, null, 0},
    };

    private static final String GOOGLE_URL = "https://www.google.com";

    private Intent mIntent;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Captor ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private ShadowPowerManager mShadowPowerManager;
    private ShadowKeyguardManager mShadowKeyguardManager;

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
        // To allow use of Origin.
        LibraryLoader.getInstance().ensureMainDexInitialized();
        IntentHandler.setTestIntentsEnabled(false);
        mIntent = new Intent();
        Context appContext = ApplicationProvider.getApplicationContext();
        mShadowPowerManager =
                Shadows.shadowOf((PowerManager) appContext.getSystemService(Context.POWER_SERVICE));
        mShadowKeyguardManager =
                Shadows.shadowOf(
                        (KeyguardManager) appContext.getSystemService(Context.KEYGUARD_SERVICE));
    }

    @Test
    @SmallTest
    public void testNewIntentInitiator() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));

        LoadUrlParams params = IntentHandler.createLoadUrlParamsForIntent(GOOGLE_URL, intent, 0);
        Assert.assertTrue(params.getInitiatorOrigin().isOpaque());

        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        params = IntentHandler.createLoadUrlParamsForIntent(GOOGLE_URL, intent, 0);
        Assert.assertNull(params.getInitiatorOrigin());
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
                    IntentHandler.GOOGLECHROME_SCHEME
                            + ExternalNavigationHandler.SELF_SCHEME_NAVIGATE_PREFIX
                            + VALID_HTTP_AND_HTTPS_URLS[i];
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
                    IntentHandler.GOOGLECHROME_SCHEME
                            + ExternalNavigationHandler.SELF_SCHEME_NAVIGATE_PREFIX
                            + REJECTED_INTENT_URLS[i];
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
    @Feature({"Android-Appbase"})
    public void testUrlFromIntent_WebappUrl() {
        Intent webappLauncherActivityIntent =
                WebappTestHelper.createMinimalWebappIntent("id", GOOGLE_URL);
        WebappLauncherActivity.LaunchData launchData =
                new WebappLauncherActivity.LaunchData(
                        "id",
                        GOOGLE_URL,
                        /* webApkPackageName= */ null,
                        /* isSplashProvidedByWebApk= */ false);
        mIntent =
                WebappLauncherActivity.createIntentToLaunchForWebapp(
                        webappLauncherActivityIntent, launchData);
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
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_customTabIntentWithSession() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context, "https://www.google.com/");
        Assert.assertTrue(
                CustomTabsConnection.getInstance()
                        .newSession(CustomTabsSessionToken.getSessionTokenFromIntent(intent)));
        Assert.assertEquals(
                "android-app://org.chromium.chrome",
                IntentHandler.getReferrerUrlIncludingExtraHeaders(intent));
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
        Assert.assertEquals(
                "X-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // MHTML type with extra headers.
        intent.setType("multipart/related");
        Assert.assertEquals(
                "Foo: bar\nX-Chrome-intent-type: multipart/related",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, "Foo: bar"));

        // Different MHTML type.
        intent.setType("message/rfc822");
        Assert.assertEquals(
                "X-Chrome-intent-type: message/rfc822",
                IntentHandler.maybeAddAdditionalContentHeaders(intent, contentUrl, null));

        // Different MHTML type with extra headers.
        intent.setType("message/rfc822");
        Assert.assertEquals(
                "Foo: bar\nX-Chrome-intent-type: message/rfc822",
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

    /** Test that IntentHandler#shouldIgnoreIntent() returns false for Webapp launch intents. */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIntentWebapp() {
        Intent webappLauncherActivityIntent =
                WebappTestHelper.createMinimalWebappIntent("id", GOOGLE_URL);
        WebappLauncherActivity.LaunchData launchData =
                new WebappLauncherActivity.LaunchData(
                        "id",
                        GOOGLE_URL,
                        /* webApkPackageName= */ null,
                        /* isSplashProvidedByWebApk= */ false);
        Intent intent =
                WebappLauncherActivity.createIntentToLaunchForWebapp(
                        webappLauncherActivityIntent, launchData);

        assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
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
        assertTrue(IntentHandler.shouldIgnoreIntent(intent, null));
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
        assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
    }

    /** Test that IntentHandler#shouldIgnoreIntent() returns false for Incognito Custom Tab Intents. */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testShouldIgnoreIncognitoIntent_customTab() {
        Intent intent = new Intent(GOOGLE_URL);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        assertFalse(IntentHandler.shouldIgnoreIntent(intent, /* isCustomTab= */ true));
    }

    @Test
    @SmallTest
    public void testIgnoreUnauthenticatedBringToFront() {
        int tabId = 1;
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
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
        intent.setComponent(
                new ComponentName(
                        ContextUtils.getApplicationContext(), IntentHandlerRobolectricTest.class));
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
            var histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            IntentHandler.SHARE_INTENT_HISTOGRAM, (int) shareCase[2]);
            Assert.assertEquals((String) shareCase[1], IntentHandler.getUrlFromShareIntent(intent));
            histogramWatcher.assertExpected((String) shareCase[0]);
        }
    }

    @Test
    @SmallTest
    public void testNewIntentInitiatorFromNewTabUrl() {
        int tabId = 1;
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentHandler.setTabId(intent, tabId);

        // Setup the tab's LoadUrlParams.
        Referrer urlReferrer = new Referrer(GOOGLE_URL, 0);
        LoadUrlParams loadUrlParams = new LoadUrlParams(GOOGLE_URL);
        loadUrlParams.setReferrer(urlReferrer);
        AsyncTabParamsManagerSingleton.getInstance()
                .add(tabId, new AsyncTabCreationParams(loadUrlParams));

        LoadUrlParams params = IntentHandler.createLoadUrlParamsForIntent(GOOGLE_URL, intent, 0);
        Assert.assertEquals("LoadUrlParams should match.", loadUrlParams, params);
        Assert.assertNotNull("The referrer should be non-null.", params.getReferrer());
    }

    @Test
    @SmallTest
    public void testScreenOffNoContext() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        mShadowPowerManager.setIsInteractive(false);
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, null));
        mShadowPowerManager.setIsInteractive(true);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
        mShadowPowerManager.setIsInteractive(false);
        intent = new Intent(Intent.ACTION_MAIN);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
        intent.setData(Uri.parse(GOOGLE_URL));
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, null));
    }

    @Test
    @SmallTest
    public void testScreenOffOneDisplay() {
        ActivityController<Activity> controller =
                Robolectric.buildActivity(
                        Activity.class,
                        null,
                        ActivityOptions.makeBasic()
                                .setLaunchDisplayId(Display.DEFAULT_DISPLAY)
                                .toBundle());
        Activity activity = controller.setup().get();

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        mShadowPowerManager.setIsInteractive(false);
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, activity));
        mShadowPowerManager.setIsInteractive(true);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, activity));
        mShadowPowerManager.setIsInteractive(false);
        intent = new Intent(Intent.ACTION_MAIN);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, activity));
        intent.setData(Uri.parse(GOOGLE_URL));
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, activity));
    }

    @Test
    @SmallTest
    public void testScreenOffTwoDisplays() {
        int extDisplayId = ShadowDisplayManager.addDisplay("");
        ActivityController<Activity> controller =
                Robolectric.buildActivity(
                        Activity.class,
                        null,
                        ActivityOptions.makeBasic().setLaunchDisplayId(extDisplayId).toBundle());
        Activity activity = controller.setup().get();
        ShadowPowerManager mShadowPowerManagerAct =
                Shadows.shadowOf((PowerManager) activity.getSystemService(Context.POWER_SERVICE));

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        // Test with main screen on
        mShadowPowerManager.setIsInteractive(true);
        mShadowPowerManagerAct.setIsInteractive(false);
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, activity));
        mShadowPowerManagerAct.setIsInteractive(true);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, activity));
        // Test with main screen off
        mShadowPowerManager.setIsInteractive(false);
        mShadowPowerManagerAct.setIsInteractive(false);
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, activity));
        mShadowPowerManagerAct.setIsInteractive(true);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, activity));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BLOCK_INTENTS_WHILE_LOCKED)
    public void testPhoneLocked() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(GOOGLE_URL));
        mShadowKeyguardManager.setKeyguardLocked(true);
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, null));
        mShadowKeyguardManager.setKeyguardLocked(false);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
        mShadowKeyguardManager.setKeyguardLocked(true);
        intent = new Intent(Intent.ACTION_MAIN);
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(intent, null));
        intent.setData(Uri.parse(GOOGLE_URL));
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(intent, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testChromeInternalUrlsAllowedFromSelf() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent trustedIntent = new Intent(Intent.ACTION_VIEW);
        trustedIntent.setData(Uri.parse("http://www.google.com"));
        trustedIntent.setPackage(context.getPackageName());
        IntentUtils.addTrustedIntentExtras(trustedIntent);

        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(trustedIntent));

        trustedIntent.setData(Uri.parse("chrome://credits"));
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(trustedIntent, null));

        trustedIntent.setData(Uri.parse("chrome-native://newtab"));
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(trustedIntent, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testChromeInternalUrlsBlockedForUntrustedSources() {
        Intent untrustedIntent = new Intent(Intent.ACTION_VIEW);
        Assert.assertFalse(IntentHandler.wasIntentSenderChrome(untrustedIntent));

        untrustedIntent.setData(Uri.parse("chrome://credits"));
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(untrustedIntent, null));

        untrustedIntent.setData(Uri.parse("chrome-native://newtab"));
        Assert.assertTrue(IntentHandler.shouldIgnoreIntent(untrustedIntent, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testChromeInternalUrlsAllowedForWhitelistedUrls() {
        Intent untrustedIntent = new Intent(Intent.ACTION_VIEW);
        Assert.assertFalse(IntentHandler.wasIntentSenderChrome(untrustedIntent));

        untrustedIntent.setData(Uri.parse("about:blank"));
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(untrustedIntent, null));

        untrustedIntent.setData(Uri.parse("about://blank"));
        Assert.assertFalse(IntentHandler.shouldIgnoreIntent(untrustedIntent, null));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testRefererUrl_signedExtraReferrer() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent trustedIntent = new Intent(Intent.ACTION_VIEW);
        trustedIntent.setData(Uri.parse(GOOGLE_URL));
        trustedIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(GOOGLE_URL));
        trustedIntent.setPackage(context.getPackageName());
        IntentUtils.addTrustedIntentExtras(trustedIntent);

        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(trustedIntent));

        Assert.assertEquals(
                GOOGLE_URL, IntentHandler.getReferrerUrlIncludingExtraHeaders(trustedIntent));
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(trustedIntent));
    }
}
