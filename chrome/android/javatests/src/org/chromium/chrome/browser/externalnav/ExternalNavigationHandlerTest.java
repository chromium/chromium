// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.test.mock.MockPackageManager;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.webapps.WebappInfo;
import org.chromium.chrome.browser.webapps.WebappScopePolicy;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.ui.base.PageTransition;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@DisableIf.Build(message = "Flaky on K - see https://crbug.com/851444",
        sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP)
public class ExternalNavigationHandlerTest {
    @Rule
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    // Expectations
    private static final int IGNORE = 0x0;
    private static final int START_INCOGNITO = 0x1;
    private static final int START_WEBAPK = 0x2;
    private static final int START_FILE = 0x4;
    private static final int START_WEBAPP_CCT = 0x8;
    private static final int START_OTHER_ACTIVITY = 0x10;
    private static final int INTENT_SANITIZATION_EXCEPTION = 0x20;
    private static final int PROXY_FOR_INSTANT_APPS = 0x40;

    private static final String SEARCH_RESULT_URL_FOR_TOM_HANKS =
            "https://www.google.com/search?q=tom+hanks";
    private static final String IMDB_WEBPAGE_FOR_TOM_HANKS = "http://m.imdb.com/name/nm0000158";
    private static final String INTENT_URL_WITH_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME =
            "intent:///name/nm0000158#Intent;scheme=imdb;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String SOME_JAVASCRIPT_PAGE = "javascript:window.open(0);";
    private static final String INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(SOME_JAVASCRIPT_PAGE) + ";end";
    private static final String IMDB_APP_INTENT_FOR_TOM_HANKS = "imdb:///name/nm0000158";
    private static final String INTENT_URL_WITH_CHAIN_FALLBACK_URL =
            "intent://scan/#Intent;scheme=zxing;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("http://url.myredirector.com/aaa") + ";end";
    private static final String ENCODED_MARKET_REFERRER =
            "_placement%3D{placement}%26network%3D{network}%26device%3D{devicemodel}";
    private static final String INTENT_APP_NOT_INSTALLED_DEFAULT_MARKET_REFERRER =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;end";
    private static final String INTENT_APP_NOT_INSTALLED_WITH_MARKET_REFERRER =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;S."
            + ExternalNavigationHandler.EXTRA_MARKET_REFERRER + "="
            + ENCODED_MARKET_REFERRER + ";end";
    private static final String INTENT_APP_PACKAGE_NAME = "com.imdb.mobile";
    private static final String YOUTUBE_URL = "http://youtube.com";
    private static final String YOUTUBE_MOBILE_URL = "http://m.youtube.com";
    private static final String YOUTUBE_PACKAGE_NAME = "youtube";

    private static final String PLUS_STREAM_URL = "https://plus.google.com/stream";
    private static final String CALENDAR_URL = "http://www.google.com/calendar";
    private static final String KEEP_URL = "http://www.google.com/keep";

    private static final String TEXT_APP_1_PACKAGE_NAME = "text_app_1";
    private static final String TEXT_APP_2_PACKAGE_NAME = "text_app_2";

    private static final String WEBAPK_SCOPE = "https://www.template.com";
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.template";

    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};

    private Context mContext;
    private final TestExternalNavigationDelegate mDelegate;
    private ExternalNavigationHandler mUrlHandler;

    public ExternalNavigationHandlerTest() {
        mDelegate = new TestExternalNavigationDelegate();
        mUrlHandler = new ExternalNavigationHandler(mDelegate);
    }

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
        mContext = new TestContext(InstrumentationRegistry.getTargetContext(), mDelegate);
        ContextUtils.initApplicationContextForTests(mContext);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.CCT_EXTERNAL_LINK_HANDLING, true);
        features.put(ChromeFeatureList.INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE, true);
        ChromeFeatureList.setTestFeatures(features);

        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    @SmallTest
    public void testStartActivityToTrustedPackageWithoutUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler handler = TabRedirectHandler.create();
        handler.updateNewUrlLoading(PageTransition.CLIENT_REDIRECT, false, false, 0, 0);

        checkUrl(YOUTUBE_URL)
                .withRedirectHandler(handler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        mDelegate.setIsCallingAppTrusted(true);

        checkUrl(YOUTUBE_URL)
                .withRedirectHandler(handler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testOrdinaryIncognitoUri() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/587306: Don't prompt the user for capturing URLs in incognito, just keep
        // it within the browser.
        checkUrl(YOUTUBE_URL)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testChromeReferrer() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/159153: Don't override http or https URLs from the NTP or bookmarks.
        checkUrl(YOUTUBE_URL)
                .withReferrer("chrome://about")
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("tel:012345678")
                .withReferrer("chrome://about")
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testForwardBackNavigation() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/164194. We shouldn't show the intent picker on
        // forwards or backwards navigations.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.LINK | PageTransition.FORWARD_BACK)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit. OAuth of native applications rely on this.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // If the page matches the referrer, then continue loading in Chrome.
        checkUrl("http://youtube.com://")
                .withReferrer(YOUTUBE_URL)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // If the page does not match the referrer, then prompt an intent.
        checkUrl("http://youtube.com://")
                .withReferrer("http://google.com")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // It doesn't make sense to allow intent picker without redirect, since form data
        // is not encoded in the intent (although, in theory, it could be passed in as
        // an extra data in the intent).
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit_NoUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // If the redirect is not associated with a user gesture, then continue loading in Chrome.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit_NoUserGesture_OnIntentRedirectChain() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = new TabRedirectHandler() {
            @Override
            public boolean isOnEffectiveIntentRedirectChain() {
                return true;
            }
        };

        // If the redirect is not associated with a user gesture but came from an incoming intent,
        // then allow those to launch external intents.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIgnore() {
        // Ensure about: URLs are not broadcast for external navigation.
        checkUrl("about:test").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("about:test")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Ensure content: URLs are not broadcast for external navigation.
        checkUrl("content:test").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("content:test")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Ensure chrome: URLs are not broadcast for external navigation.
        checkUrl("chrome://history").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("chrome://history")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Ensure chrome-native: URLs are not broadcast for external navigation.
        checkUrl("chrome-native://newtab").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl("chrome-native://newtab")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testPageTransitionType() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // Non-link page transition type are ignored.
        checkUrl(YOUTUBE_URL)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl(YOUTUBE_URL)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/143118 - Don't show the picker for directly typed URLs, unless
        // the URL results in a redirect.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.TYPED)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // http://crbug.com/162106 - Don't show the picker on reload.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.RELOAD)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testWtai() {
        // Start the telephone application with the given number.
        checkUrl("wtai://wp/mc;0123456789")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY | INTENT_SANITIZATION_EXCEPTION);

        // These two cases are currently unimplemented.
        checkUrl("wtai://wp/sd;0123456789")
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);
        checkUrl("wtai://wp/ap;0123456789")
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);

        // Ignore other WTAI urls.
        checkUrl("wtai://wp/invalid")
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);
    }

    @Test
    @SmallTest
    public void testRedirectToMarketWithReferrer() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_APP_NOT_INSTALLED_WITH_MARKET_REFERRER)
                .withReferrer(KEEP_URL)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Uri uri = mDelegate.startActivityIntent.getData();
        Assert.assertEquals("market", uri.getScheme());
        Assert.assertEquals(Uri.decode(ENCODED_MARKET_REFERRER), uri.getQueryParameter("referrer"));
        Assert.assertEquals(Uri.parse(KEEP_URL),
                mDelegate.startActivityIntent.getParcelableExtra(Intent.EXTRA_REFERRER));
    }

    @Test
    @SmallTest
    public void testRedirectToMarketWithoutReferrer() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_APP_NOT_INSTALLED_DEFAULT_MARKET_REFERRER)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Uri uri = mDelegate.startActivityIntent.getData();
        Assert.assertEquals("market", uri.getScheme());
        Assert.assertEquals(getPackageName(), uri.getQueryParameter("referrer"));
    }

    @Test
    @SmallTest
    public void testExternalUri() {
        checkUrl("tel:012345678")
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testTypedRedirectToExternalProtocol() {
        // http://crbug.com/169549
        checkUrl("market://1234")
                .withPageTransition(PageTransition.TYPED)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/709217
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FROM_ADDRESS_BAR)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/143118
        checkUrl("market://1234")
                .withPageTransition(PageTransition.TYPED)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIncomingIntentRedirect() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        int transitionTypeIncomingIntent = PageTransition.LINK
                | PageTransition.FROM_API;
        // http://crbug.com/149218
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transitionTypeIncomingIntent)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // http://crbug.com/170925
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transitionTypeIncomingIntent)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentScheme() {
        String url = "intent:wtai://wp/#Intent;action=android.settings.SETTINGS;"
                + "component=package/class;end";
        String urlWithSel = "intent:wtai://wp/#Intent;SEL;action=android.settings.SETTINGS;"
                + "component=package/class;end";

        checkUrl(url).expecting(
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);

        // http://crbug.com/370399
        checkUrl(urlWithSel)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testYouTubePairingCode() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        int transitionTypeIncomingIntent = PageTransition.LINK
                | PageTransition.FROM_API;
        final String[] goodUrls = {"http://m.youtube.com/watch?v=1234&pairingCode=5678",
                "youtube.com?pairingCode=xyz", "youtube.com/tv?pairingCode=xyz",
                "youtube.com/watch?v=1234&version=3&autohide=1&pairingCode=xyz",
                "youtube.com/watch?v=1234&pairingCode=xyz&version=3&autohide=1"};
        final String[] badUrls = {"youtube.com.foo.com/tv?pairingCode=xyz",
                "youtube.com.foo.com?pairingCode=xyz",
                "youtube.com/watch?v=tEsT&version=3&autohide=1&pairingCode=",
                "youtube.com&pairingCode=xyz",
                "youtube.com/watch?v=tEsT?version=3&pairingCode=&autohide=1"};

        // Make sure we don't override when faced with valid pairing code URLs.
        for (String url : goodUrls) {
            // http://crbug/386600 - it makes no sense to switch activities for pairing code URLs.
            checkUrl(url).withIsRedirect(true).expecting(
                    OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

            checkUrl(url)
                    .withPageTransition(transitionTypeIncomingIntent)
                    .withIsRedirect(true)
                    .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        }

        // The pairing code URL regex shouldn't cause NO_OVERRIDE on invalid URLs.
        for (String url : badUrls) {
            checkUrl(url).withIsRedirect(true).expecting(
                    OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);

            checkUrl(url)
                    .withPageTransition(transitionTypeIncomingIntent)
                    .withIsRedirect(true)
                    .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                            START_OTHER_ACTIVITY);
        }
    }

    @Test
    @SmallTest
    public void
    testInitialIntent() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();
        Intent ytIntent = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        int transTypeLinkFromIntent = PageTransition.LINK
                | PageTransition.FROM_API;

        // Ignore if url is redirected, transition type is IncomingIntent and a new intent doesn't
        // have any new resolver.
        redirectHandler.updateIntent(ytIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Do not ignore if a new intent has any new resolver.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // Do not ignore if a new intent cannot be handled by Chrome.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl("intent://myownurl")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void
    testInitialIntentHeadingToChrome() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.setPackage(mContext.getPackageName());
        int transTypeLinkFromIntent = PageTransition.LINK
                | PageTransition.FROM_API;

        // Ignore if an initial Intent was heading to Chrome.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Do not ignore if the URI has an external protocol.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl("market://1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void
    testIntentForCustomTab() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // In Custom Tabs, if the first url is not a redirect, stay in chrome.
        Intent barIntent = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        barIntent.putExtra(CustomTabsIntent.EXTRA_SESSION, "");
        barIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(barIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // In Custom Tabs, if the first url is a redirect, don't allow it to intent out.
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.putExtra(CustomTabsIntent.EXTRA_SESSION, "");
        fooIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // In Custom Tabs, if the external handler extra is present, intent out if the first
        // url is a redirect.
        Intent extraIntent2 = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        extraIntent2.putExtra(CustomTabsIntent.EXTRA_SESSION, "");
        extraIntent2.putExtra(
                CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        extraIntent2.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(extraIntent2);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Intent extraIntent3 = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        extraIntent3.putExtra(CustomTabsIntent.EXTRA_SESSION, "");
        extraIntent3.putExtra(
                CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        extraIntent3.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(extraIntent3);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // External intent for a user-initiated navigation should always be allowed.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        // Simulate a real user navigation.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true,
                SystemClock.elapsedRealtime() + 1, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.LINK)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void
    testInstantAppsIntent_customTabRedirect() throws Exception {
        TabRedirectHandler redirectHandler = TabRedirectHandler.create();
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // In Custom Tabs, if the first url is a redirect, don't allow it to intent out, unless
        // the redirect is going to Instant Apps.
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.putExtra(CustomTabsIntent.EXTRA_SESSION, "");
        fooIntent.putExtra(CustomTabsIntent.EXTRA_ENABLE_INSTANT_APPS, true);
        fooIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://instantappenabled.com")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);
    }

    @Test
    @SmallTest
    public void testInstantAppsIntent_incomingIntentRedirect() throws Exception {
        int transTypeLinkFromIntent = PageTransition.LINK
                | PageTransition.FROM_API;
        TabRedirectHandler redirectHandler = TabRedirectHandler.create();
        Intent fooIntent = Intent.parseUri("http://instantappenabled.com",
                Intent.URI_INTENT_SCHEME);
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://goo.gl/1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);

        // URL that cannot be handled with instant apps should stay in Chrome.
        mDelegate.setCanHandleWithInstantApp(false);
        checkUrl("http://goo.gl/1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testInstantAppsIntent_handleNavigation() {
        mDelegate.setCanHandleWithInstantApp(false);
        checkUrl("http://maybeinstantapp.com")
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://maybeinstantapp.com")
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);
    }

    @Test
    @SmallTest
    public void
    testInstantAppsIntent_serpReferrer() {
        String intentUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                + "package=com.google.android.instantapps.supervisor;"
                + "action=com.google.android.instantapps.START;"
                + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                + "https%3A%2F%2Fwww.google.com;end";
        mDelegate.setIsSerpReferrer(true);
        checkUrl(intentUrl)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY | PROXY_FOR_INSTANT_APPS);
        Assert.assertTrue(mDelegate.startActivityIntent.hasExtra(
                InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));

        // Check that we block all instant app intent:// URLs not from SERP
        mDelegate.setIsSerpReferrer(false);
        checkUrl(intentUrl)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Check that IS_GOOGLE_SEARCH_REFERRER param is stripped on non-supervisor intents.
        mDelegate.setIsSerpReferrer(true);
        String nonSupervisor = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                + "package=com.imdb;action=com.google.VIEW;"
                + "S.com.google.android.gms.instantapps.IS_GOOGLE_SEARCH_REFERRER="
                + "true;S.android.intent.extra.REFERRER_NAME="
                + "https%3A%2F%2Fwww.google.com;end";
        checkUrl(nonSupervisor)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertFalse(mDelegate.startActivityIntent.hasExtra(
                InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));

        // Check that Supervisor is detected by action even without package
        for (String action : SUPERVISOR_START_ACTIONS) {
            String intentWithoutPackage = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                    + "action=" + action + ";"
                    + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                    + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                    + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                    + "https%3A%2F%2Fwww.google.com;end";
            mDelegate.setIsSerpReferrer(false);
            checkUrl(intentWithoutPackage)
                    .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        }
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionSucceeds() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Intent invokedIntent = mDelegate.startActivityIntent;
        Assert.assertEquals(IMDB_APP_INTENT_FOR_TOM_HANKS, invokedIntent.getData().toString());
        Assert.assertNull("The invoked intent should not have browser_fallback_url\n",
                invokedIntent.getStringExtra(ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL));
        Assert.assertNull(mDelegate.getNewUrlAfterClobbering());
        Assert.assertNull(mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void
    testFallbackUrl_IntentResolutionSucceedsInIncognito() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        // Expect that the user is prompted to leave incognito mode.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withIsIncognito(true)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                        START_INCOGNITO | START_OTHER_ACTIVITY);

        Assert.assertNull(mDelegate.getNewUrlAfterClobbering());
        Assert.assertNull(mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackToWebApk() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME)
                              .withIsWebApk(true));
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_DontFallbackToWebApkMultipleHandlers() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME)
                              .withIsWebApk(true));
        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, TEXT_APP_1_PACKAGE_NAME));
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        Assert.assertEquals(
                SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionFails() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        // When intent resolution fails, we should not start an activity, but instead clobber
        // the current tab.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        Assert.assertEquals(
                SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackToMarketApp() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        String intent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                + "&referrer=mypage;end";
        checkUrl(intent)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                mDelegate.startActivityIntent.getDataString());

        String intentNoRef = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile;end";
        checkUrl(intentNoRef)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=" + getPackageName(),
                mDelegate.startActivityIntent.getDataString());

        String intentBadUrl = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/search?q=pub:imdb;end";
        checkUrl(intentBadUrl)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_RedirectToIntentToMarket() {
        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        checkUrl("http://goo.gl/abcdefg")
                .withPageTransition(PageTransition.TYPED)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        String realIntent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                + "&referrer=mypage;end";

        checkUrl(realIntent)
                .withPageTransition(PageTransition.LINK)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                mDelegate.startActivityIntent.getDataString());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionFailsWithoutPackageName() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        // Fallback URL should work even when package name isn't given.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        Assert.assertEquals(
                SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackShouldNotWarnOnIncognito() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        Assert.assertEquals(
                SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IgnoreJavascriptFallbackUrl() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        // Will be redirected to market since package is given.
        checkUrl(INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                        START_INCOGNITO | START_OTHER_ACTIVITY);

        Intent invokedIntent = mDelegate.startActivityIntent;
        Assert.assertTrue(invokedIntent.getData().toString().startsWith("market://"));
        Assert.assertEquals(null, mDelegate.getNewUrlAfterClobbering());
        Assert.assertEquals(null, mDelegate.getReferrerUrlForClobbering());
    }

    @Test
    @SmallTest
    public void testFallback_UseFallbackUrlForRedirectionFromTypedInUrl() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        checkUrl("http://goo.gl/abcdefg")
                .withPageTransition(PageTransition.TYPED)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        // Now the user opens a link.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIgnoreEffectiveRedirectFromIntentFallbackUrl() {
        // We cannot resolve any intent, so fall-back URL will be used.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        checkUrl(INTENT_URL_WITH_CHAIN_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        // As a result of intent resolution fallback, we have clobberred the current tab.
        // The fall-back URL was HTTP-schemed, but it was effectively redirected to a new intent
        // URL using javascript. However, we do not allow chained fallback intent, so we do NOT
        // override URL loading here.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        // Now enough time (2 seconds) have passed.
        // New URL loading should not be affected.
        // (The URL happened to be the same as previous one.)
        // TODO(changwan): this is not likely cause flakiness, but it may be better to refactor
        // systemclock or pass the new time as parameter.
        long lastUserInteractionTimeInMillis = SystemClock.elapsedRealtime() + 2 * 1000L;
        redirectHandler.updateNewUrlLoading(
                PageTransition.LINK, false, true, lastUserInteractionTimeInMillis, 1);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
    }

    @Test
    @SmallTest
    public void testIgnoreEffectiveRedirectFromUserTyping() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(PageTransition.TYPED)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(PageTransition.TYPED)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNavigationFromLinkWithoutUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void
    testChromeAppInBackground() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.setIsChromeAppInForeground(false);
        checkUrl(YOUTUBE_URL).expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNotChromeAppInForegroundRequired() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.setIsChromeAppInForeground(false);
        checkUrl(YOUTUBE_URL)
                .withChromeAppInForegroundRequired(false)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testCreatesIntentsToOpenInNewTab() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        mUrlHandler = new ExternalNavigationHandler(mDelegate);
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(YOUTUBE_MOBILE_URL, false)
                        .setOpenInNewTab(true)
                        .build();
        @OverrideUrlLoadingResult
        int result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, result);
        Assert.assertTrue(mDelegate.startActivityIntent != null);
        Assert.assertTrue(
                mDelegate.startActivityIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
    }

    @Test
    @SmallTest
    public void testCanExternalAppHandleUrl() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);
        mDelegate.add(new IntentActivity("some_app", "someapp"));

        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl("some_app://some_app.com/"));

        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl("wtai://wp/mc;0123456789"));
        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl(
                "intent:/#Intent;scheme=no_app;package=com.no_app;end"));
        Assert.assertFalse(mUrlHandler.canExternalAppHandleUrl("no_app://no_app.com/"));
    }

    @Test
    @SmallTest
    public void testPlusAppRefresh() {
        mDelegate.add(new IntentActivity(PLUS_STREAM_URL, "plus"));

        checkUrl(PLUS_STREAM_URL)
                .withReferrer(PLUS_STREAM_URL)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testSameDomainDifferentApps() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL)
                .withReferrer(KEEP_URL)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testFormSubmitSameDomain() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL)
                .withReferrer(KEEP_URL)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testBackgroundTabNavigation() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        checkUrl(YOUTUBE_URL)
                .withIsBackgroundTabNavigation(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testPdfDownloadHappensInChrome() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL + "/file.pdf")
            .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIntentToPdfFileOpensApp() {
        checkUrl("intent://yoursite.com/mypdf.pdf#Intent;action=VIEW;category=BROWSABLE;"
                + "scheme=http;package=com.adobe.reader;end;")
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentWithMissingReferrer() {
        mDelegate.add(new IntentActivity("http://refertest.com", "refertest"));
        mDelegate.add(new IntentActivity("https://refertest.com", "refertest"));

        // http://crbug.com/702089: Don't override links within the same host/domain.
        // This is an issue for HTTPS->HTTP because there's no referrer, so we fall back on the
        // WebContents.lastCommittedUrl.

        checkUrl("http://refertest.com")
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        mDelegate.setPreviousUrl("https://refertest.com");
        checkUrl("http://refertest.com").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testReferrerExtra() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        String referrer = "http://www.google.com";
        checkUrl("http://youtube.com:90/foo/bar")
                .withReferrer(referrer)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertEquals(Uri.parse(referrer),
                mDelegate.startActivityIntent.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertEquals(
                1, mDelegate.startActivityIntent.getIntExtra(IntentHandler.EXTRA_REFERRER_ID, 0));
    }

    @Test
    @SmallTest
    public void testNavigationFromReload() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.RELOAD, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNavigationWithForwardBack() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        TabRedirectHandler redirectHandler = TabRedirectHandler.create();

        redirectHandler.updateNewUrlLoading(
                PageTransition.FORM_SUBMIT | PageTransition.FORWARD_BACK, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SuppressLint("SdCardPath")
    @SmallTest
    public void testFileAccess() {
        String fileUrl = "file:///sdcard/Downloads/test.html";

        mDelegate.shouldRequestFileAccess = false;
        // Verify no overrides if file access is allowed (under different load conditions).
        checkUrl(fileUrl).expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.RELOAD)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.AUTO_TOPLEVEL)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        mDelegate.shouldRequestFileAccess = true;
        // Verify that the file intent action is triggered if file access is not allowed.
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.AUTO_TOPLEVEL)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION, START_FILE);
    }

    @Test
    @SmallTest
    public void testSms_DispatchIntentToDefaultSmsApp() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        mDelegate.defaultSmsPackageName = TEXT_APP_2_PACKAGE_NAME;

        checkUrl("sms:+012345678?body=hello%20there")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertEquals(TEXT_APP_2_PACKAGE_NAME, mDelegate.startActivityIntent.getPackage());
    }

    @Test
    @SmallTest
    public void testSms_DefaultSmsAppDoesNotHandleIntent() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        // Note that this package does not resolve the intent.
        mDelegate.defaultSmsPackageName = "text_app_3";

        checkUrl("sms:+012345678?body=hello%20there")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertNull(mDelegate.startActivityIntent.getPackage());
    }

    @Test
    @SmallTest
    public void testSms_DispatchIntentSchemedUrlToDefaultSmsApp() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        mDelegate.defaultSmsPackageName = TEXT_APP_2_PACKAGE_NAME;

        checkUrl("intent://012345678?body=hello%20there/#Intent;scheme=sms;end")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertEquals(TEXT_APP_2_PACKAGE_NAME, mDelegate.startActivityIntent.getPackage());
    }

    /**
     * Test that tapping a link which falls into the scope of the current webapp keeps the user in
     * the webapp.
     */
    @Test
    @SmallTest
    public void testLaunchWebApp_StayInSameWebApp() {
        final String twaScope = "https://my_twa.org";
        final String twaPackageName = "org.my_twa";
        mDelegate.add(new IntentActivity(twaScope, twaPackageName)
                              .withWebappScopePolicy(WebappScopePolicy.Type.STRICT));
        mDelegate.setReferrerWebappPackageName(twaPackageName);

        checkUrl(twaScope + "/new.html").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    /**
     * Test that when a webapp with "STRICT scope policy" is navigated outside of the webapp's scope
     * by "tapping a link" that a Chrome Custom Tab is not launched.
     */
    @Test
    @SmallTest
    public void testLeaveStrictWebapp_LinkOutOfScope() {
        final String twaScope = "https://my_twa.org";
        final String twaPackageName = "org.my_twa";
        mDelegate.add(new IntentActivity(twaScope, twaPackageName)
                              .withWebappScopePolicy(WebappScopePolicy.Type.STRICT));
        mDelegate.setReferrerWebappPackageName(twaPackageName);

        checkUrl(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    /**
     * Test that when a webapp with "STRICT scope policy" is navigated outside of the webapp's scope
     * via "JavaScript while the webapp is in the background" that a CCT is not launched (as not to
     * move the webapp to the foreground and annoy the user)
     */
    @Test
    @SmallTest
    public void testLeaveStrictWebapp_JSBackgroundNavOutOfScope() {
        final String twaScope = "https://my_twa.org";
        final String twaPackageName = "org.my_twa";
        mDelegate.add(new IntentActivity(twaScope, twaPackageName)
                              .withWebappScopePolicy(WebappScopePolicy.Type.STRICT));
        mDelegate.setReferrerWebappPackageName(twaPackageName);

        checkUrl(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsBackgroundTabNavigation(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    /**
     * Test that when a webapp with "LEGACY scope policy" is navigated outside of the webapp's scope
     * by "tapping a link" that a CCT is not launched.
     */
    @Test
    @SmallTest
    public void testLeaveLegacyWebapp_LinkOutOfScope() {
        final String twaScope = "https://my_twa.org";
        final String twaPackageName = "org.my_twa";
        mDelegate.add(new IntentActivity(twaScope, twaPackageName)
                              .withWebappScopePolicy(WebappScopePolicy.Type.LEGACY));
        mDelegate.setReferrerWebappPackageName(twaPackageName);

        checkUrl(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    /**
     * Test that tapping a link which falls solely in the scope of a WebAPK launches a WebAPK
     * without showing the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_BypassIntentPicker() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, WEBAPK_PACKAGE_NAME).withIsWebApk(true));

        checkUrl(WEBAPK_SCOPE)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    /**
     * Test that tapping a link which falls in the scope of multiple intent handlers, one of which
     * is a WebAPK, shows the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_ShowIntentPickerMultipleIntentHandlers() {
        final String scope = "https://www.webapk.with.native.com";
        mDelegate.add(
                new IntentActivity(scope, "org.chromium.webapk.with.native").withIsWebApk(true));
        mDelegate.add(new IntentActivity(scope, "com.webapk.with.native.android"));

        checkUrl(scope).expecting(
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);
    }

    /**
     * Test that tapping a link which falls solely into the scope of a different WebAPK launches a
     * WebAPK without showing the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_BypassIntentPickerFromAnotherWebApk() {
        final String scope1 = "https://www.webapk.with.native.com";
        final String scope1WebApkPackageName = "org.chromium.webapk.with.native";
        final String scope1NativeAppPackageName = "com.webapk.with.native.android";
        final String scope2 = "https://www.template.com";
        mDelegate.add(new IntentActivity(scope1, scope1WebApkPackageName).withIsWebApk(true));
        mDelegate.add(new IntentActivity(scope1, scope1NativeAppPackageName));
        mDelegate.add(new IntentActivity(scope2, WEBAPK_PACKAGE_NAME).withIsWebApk(true));
        mDelegate.setReferrerWebappPackageName(scope1WebApkPackageName);

        checkUrl(scope2)
                .withReferrer(scope1)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    /**
     * Test that a link which falls into the scope of an invalid WebAPK (e.g. it was incorrectly
     * signed) does not get any special WebAPK handling. The first time that the user taps on the
     * link, the intent picker should be shown.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_ShowIntentPickerInvalidWebApk() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, WEBAPK_PACKAGE_NAME).withIsWebApk(false));
        checkUrl(WEBAPK_SCOPE)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testMarketIntent_MarketInstalled() {
        checkUrl("market://1234")
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertTrue(mDelegate.startActivityIntent.getScheme().startsWith("market"));
    }

    @Test
    @SmallTest
    public void testMarketIntent_MarketNotInstalled() {
        mDelegate.setCanResolveActivityForMarket(false);
        checkUrl("market://1234").expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testMarketIntent_ShowDialogIncognitoMarketInstalled() {
        checkUrl("market://1234")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                        START_INCOGNITO | START_OTHER_ACTIVITY);

        Assert.assertTrue(mDelegate.startIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testMarketIntent_DontShowDialogIncognitoMarketNotInstalled() {
        mDelegate.setCanResolveActivityForMarket(false);
        checkUrl("market://1234")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        Assert.assertFalse(mDelegate.startIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testUserGesture_Regular() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertTrue(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(
                mDelegate.startActivityIntent));
        Assert.assertFalse(mDelegate.startIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testUserGesture_Incognito() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withHasUserGesture(true)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                        START_INCOGNITO | START_OTHER_ACTIVITY);
        Assert.assertTrue(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(
                mDelegate.startActivityIntent));
        Assert.assertTrue(mDelegate.startIncognitoIntentCalled);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo ai = new ActivityInfo();
        ai.packageName = packageName;
        ai.name = "Name: " + packageName;
        ResolveInfo ri = new ResolveInfo();
        ri.activityInfo = ai;
        return ri;
    }

    private static ResolveInfo newSpecializedResolveInfo(
            String packageName, IntentActivity activity) {
        ResolveInfo info = newResolveInfo(packageName);
        info.filter = new IntentFilter(Intent.ACTION_VIEW);
        info.filter.addDataAuthority(activity.mUrlPrefix, null);
        return info;
    }

    private static WebappInfo newWebappInfoFromScope(String scope) {
        Intent webappIntent = WebappTestHelper.createMinimalWebappIntent("" /* id */, "" /* url */);
        webappIntent.putExtra(ShortcutHelper.EXTRA_SCOPE, scope);
        return WebappInfo.create(webappIntent);
    }

    private static class IntentActivity {
        private String mUrlPrefix;
        private String mPackageName;
        private boolean mIsWebApk;
        private @WebappScopePolicy.Type int mWebappScopePolicy;

        public IntentActivity(String urlPrefix, String packageName) {
            mUrlPrefix = urlPrefix;
            mPackageName = packageName;
            mWebappScopePolicy = WebappScopePolicy.Type.LEGACY;
        }

        public IntentActivity withIsWebApk(boolean isWebApk) {
            mIsWebApk = isWebApk;
            mWebappScopePolicy = WebappScopePolicy.Type.STRICT;
            return this;
        }

        public IntentActivity withWebappScopePolicy(@WebappScopePolicy.Type int policy) {
            mWebappScopePolicy = policy;
            return this;
        }

        public String urlPrefix() {
            return mUrlPrefix;
        }

        public String packageName() {
            return mPackageName;
        }

        public boolean isWebApk() {
            return mIsWebApk;
        }

        public @WebappScopePolicy.Type int webappScopePolicy() {
            return mWebappScopePolicy;
        }

        public boolean isSpecialized() {
            // Specialized if URL prefix is more than just a scheme.
            return Pattern.compile("[^:/]+://.+").matcher(mUrlPrefix).matches();
        }
    }

    private static class TestExternalNavigationDelegate implements ExternalNavigationDelegate {
        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent) {
            List<ResolveInfo> list = new ArrayList<>();
            String dataString = intent.getDataString();
            if (dataString.startsWith("http://") || dataString.startsWith("https://")) {
                list.add(newResolveInfo("chrome"));
            }
            for (IntentActivity intentActivity : mIntentActivities) {
                if (dataString.startsWith(intentActivity.urlPrefix())) {
                    list.add(newSpecializedResolveInfo(
                            intentActivity.packageName(), intentActivity));
                }
            }
            if (!list.isEmpty()) return list;

            String schemeString = intent.getData().getScheme();
            boolean isMarketScheme = schemeString != null && schemeString.startsWith("market");
            if (mCanResolveActivityForMarket && isMarketScheme) {
                list.add(newResolveInfo("market"));
                return list;
            }
            if (mCanResolveActivityForExternalSchemes && !isMarketScheme) {
                list.add(newResolveInfo(intent.getData().getScheme()));
            }
            return list;
        }

        @Override
        public boolean willChromeHandleIntent(Intent intent) {
            String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
            if (chromePackageName.equals(intent.getPackage())
                    || (intent.getComponent() != null
                               && chromePackageName.equals(
                                          intent.getComponent().getPackageName()))) {
                return true;
            }

            List<ResolveInfo> resolveInfos = queryIntentActivities(intent);
            return resolveInfos.size() == 1
                    && resolveInfos.get(0).activityInfo.packageName.contains("chrome");
        }

        @Override
        public @WebappScopePolicy.NavigationDirective int applyWebappScopePolicyForUrl(String url) {
            for (IntentActivity intentActivity : mIntentActivities) {
                if (intentActivity.packageName().equals(mReferrerWebappPackageName)) {
                    WebappInfo info = newWebappInfoFromScope(intentActivity.urlPrefix());
                    return WebappScopePolicy.applyPolicyForNavigationToUrl(
                            intentActivity.webappScopePolicy(), info, url);
                }
            }
            return WebappScopePolicy.NavigationDirective.NORMAL_BEHAVIOR;
        }

        @Override
        public int countSpecializedHandlers(List<ResolveInfo> infos) {
            int count = 0;
            List<IntentActivity> matchingIntentActivities = findMatchingIntentActivities(infos);
            for (IntentActivity intentActivity : matchingIntentActivities) {
                if (intentActivity.isSpecialized()) {
                    ++count;
                }
            }
            return count;
        }

        private ArrayList<IntentActivity> findMatchingIntentActivities(List<ResolveInfo> infos) {
            ArrayList<IntentActivity> outList = new ArrayList<IntentActivity>();
            for (ResolveInfo info : infos) {
                String packageName = info.activityInfo.packageName;
                for (IntentActivity intentActivity : mIntentActivities) {
                    if (intentActivity.packageName().equals(packageName)) {
                        outList.add(intentActivity);
                    }
                }
            }
            return outList;
        }

        @Override
        public void startActivity(Intent intent, boolean proxy) {
            startActivityIntent = intent;
        }

        @Override
        public boolean startActivityIfNeeded(Intent intent, boolean proxy) {
            // For simplicity, don't distinguish between startActivityIfNeeded and startActivity
            // until a test requires this distinction.
            startActivityIntent = intent;
            mCalledWithProxy = proxy;
            return true;
        }

        @Override
        public boolean startIncognitoIntent(Intent intent, String referrerUrl, String fallbackUrl,
                Tab tab, boolean needsToCloseTab, boolean proxy) {
            startActivityIntent = intent;
            startIncognitoIntentCalled = true;
            return true;
        }

        @Override
        public boolean shouldRequestFileAccess(String url) {
            return shouldRequestFileAccess;
        }

        @Override
        public void startFileIntent(Intent intent, String referrerUrl, boolean needsToCloseTab) {
            startFileIntentCalled = true;
        }

        @Override
        public @OverrideUrlLoadingResult int clobberCurrentTab(String url, String referrerUrl) {
            mNewUrlAfterClobbering = url;
            mReferrerUrlForClobbering = referrerUrl;
            return OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB;
        }

        @Override
        public void maybeSetWindowId(Intent intent) {
        }

        @Override
        public void maybeRecordAppHandlersInIntent(Intent intent, List<ResolveInfo> info) {
        }

        @Override
        public boolean isChromeAppInForeground() {
            return mIsChromeAppInForeground;
        }

        @Override
        public String getDefaultSmsPackageName() {
            return defaultSmsPackageName;
        }

        @Override
        public boolean isPdfDownload(String url) {
            return url.endsWith(".pdf");
        }

        @Override
        public boolean maybeLaunchInstantApp(
                String url, String referrerUrl, boolean isIncomingRedirect) {
            return mCanHandleWithInstantApp;
        }

        @Override
        public boolean isSerpReferrer() {
            return mIsSerpReferrer;
        }

        @Override
        public String getPreviousUrl() {
            return mPreviousUrl;
        }

        @Override
        public boolean isIntentForTrustedCallingApp(Intent intent) {
            return mIsCallingAppTrusted;
        }

        @Override
        public boolean isValidWebApk(String packageName) {
            for (IntentActivity activity : mIntentActivities) {
                if (activity.packageName().equals(packageName)) {
                    return activity.isWebApk();
                }
            }
            return false;
        }

        public void reset() {
            startActivityIntent = null;
            startIncognitoIntentCalled = false;
            startFileIntentCalled = false;
            startWebappCctIntentCalled = false;
            mCalledWithProxy = false;
        }

        public void add(IntentActivity handler) {
            mIntentActivities.add(handler);
        }

        public void setCanResolveActivityForExternalSchemes(boolean value) {
            mCanResolveActivityForExternalSchemes = value;
        }

        public void setCanResolveActivityForMarket(boolean value) {
            mCanResolveActivityForMarket = value;
        }

        public String getNewUrlAfterClobbering() {
            return mNewUrlAfterClobbering;
        }

        public String getReferrerUrlForClobbering() {
            return mReferrerUrlForClobbering;
        }

        public void setIsChromeAppInForeground(boolean value) {
            mIsChromeAppInForeground = value;
        }

        public void setReferrerWebappPackageName(String webappPackageName) {
            mReferrerWebappPackageName = webappPackageName;
        }

        public String getReferrerWebappPackageName() {
            return mReferrerWebappPackageName;
        }

        public void setCanHandleWithInstantApp(boolean value) {
            mCanHandleWithInstantApp = value;
        }

        public void setIsSerpReferrer(boolean value) {
            mIsSerpReferrer = value;
        }

        public void setPreviousUrl(String value) {
            mPreviousUrl = value;
        }

        public void setIsCallingAppTrusted(boolean trusted) {
            mIsCallingAppTrusted = trusted;
        }

        public Intent startActivityIntent;
        public boolean startIncognitoIntentCalled;
        public boolean startFileIntentCalled;
        public boolean startWebappCctIntentCalled;
        public String defaultSmsPackageName;

        private String mReferrerWebappPackageName;

        private ArrayList<IntentActivity> mIntentActivities = new ArrayList<IntentActivity>();
        private boolean mCanResolveActivityForExternalSchemes = true;
        private boolean mCanResolveActivityForMarket = true;
        private String mNewUrlAfterClobbering;
        private String mReferrerUrlForClobbering;
        private boolean mCanHandleWithInstantApp;
        private boolean mIsSerpReferrer;
        private String mPreviousUrl;
        public boolean mCalledWithProxy;
        public boolean mIsChromeAppInForeground = true;
        private boolean mIsCallingAppTrusted;

        public boolean shouldRequestFileAccess;
    }

    private void checkIntentSanity(Intent intent, String name) {
        Assert.assertTrue("The invoked " + name + " doesn't have the BROWSABLE category set\n",
                intent.hasCategory(Intent.CATEGORY_BROWSABLE));
        Assert.assertNull("The invoked " + name + " should not have a Component set\n",
                intent.getComponent());
    }

    private ExternalNavigationTestParams checkUrl(String url) {
        return new ExternalNavigationTestParams(url);
    }

    private class ExternalNavigationTestParams {
        private final String mUrl;

        private String mReferrerUrl;
        private boolean mIsIncognito;
        private int mPageTransition = PageTransition.LINK;
        private boolean mIsRedirect;
        private boolean mChromeAppInForegroundRequired = true;
        private boolean mIsBackgroundTabNavigation;
        private boolean mHasUserGesture;
        private TabRedirectHandler mRedirectHandler;

        private ExternalNavigationTestParams(String url) {
            mUrl = url;
        }

        public ExternalNavigationTestParams withReferrer(String referrerUrl) {
            mReferrerUrl = referrerUrl;
            return this;
        }

        public ExternalNavigationTestParams withIsIncognito(boolean isIncognito) {
            mIsIncognito = isIncognito;
            return this;
        }

        public ExternalNavigationTestParams withPageTransition(int pageTransition) {
            mPageTransition = pageTransition;
            return this;
        }

        public ExternalNavigationTestParams withIsRedirect(boolean isRedirect) {
            mIsRedirect = isRedirect;
            return this;
        }

        public ExternalNavigationTestParams withHasUserGesture(boolean hasGesture) {
            mHasUserGesture = hasGesture;
            return this;
        }

        public ExternalNavigationTestParams withChromeAppInForegroundRequired(
                boolean foregroundRequired) {
            mChromeAppInForegroundRequired = foregroundRequired;
            return this;
        }

        public ExternalNavigationTestParams withIsBackgroundTabNavigation(
                boolean isBackgroundTabNavigation) {
            mIsBackgroundTabNavigation = isBackgroundTabNavigation;
            return this;
        }

        public ExternalNavigationTestParams withRedirectHandler(TabRedirectHandler handler) {
            mRedirectHandler = handler;
            return this;
        }

        public void expecting(
                @OverrideUrlLoadingResult int expectedOverrideResult, int otherExpectation) {
            boolean expectStartIncognito = (otherExpectation & START_INCOGNITO) != 0;
            boolean expectStartActivity =
                    (otherExpectation & (START_WEBAPK | START_OTHER_ACTIVITY)) != 0;
            boolean expectStartWebApk = (otherExpectation & START_WEBAPK) != 0;
            boolean expectStartWebappCct = (otherExpectation & START_WEBAPP_CCT) != 0;
            boolean expectStartOtherActivity = (otherExpectation & START_OTHER_ACTIVITY) != 0;
            boolean expectStartFile = (otherExpectation & START_FILE) != 0;
            boolean expectSaneIntent = expectStartOtherActivity
                    && (otherExpectation & INTENT_SANITIZATION_EXCEPTION) == 0;
            boolean expectProxyForIA = (otherExpectation & PROXY_FOR_INSTANT_APPS) != 0;

            mDelegate.reset();

            ExternalNavigationParams params =
                    new ExternalNavigationParams
                            .Builder(mUrl, mIsIncognito, mReferrerUrl, mPageTransition, mIsRedirect)
                            .setApplicationMustBeInForeground(mChromeAppInForegroundRequired)
                            .setRedirectHandler(mRedirectHandler)
                            .setIsBackgroundTabNavigation(mIsBackgroundTabNavigation)
                            .setIsMainFrame(true)
                            .setNativeClientPackageName(mDelegate.getReferrerWebappPackageName())
                            .setHasUserGesture(mHasUserGesture)
                            .build();
            @OverrideUrlLoadingResult
            int result = mUrlHandler.shouldOverrideUrlLoading(params);
            boolean startActivityCalled = false;
            boolean startWebApkCalled = false;
            if (mDelegate.startActivityIntent != null) {
                startActivityCalled = true;
                String packageName = mDelegate.startActivityIntent.getPackage();
                if (packageName != null) {
                    startWebApkCalled =
                            packageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX);
                }
            }

            Assert.assertEquals(expectedOverrideResult, result);
            Assert.assertEquals(expectStartIncognito, mDelegate.startIncognitoIntentCalled);
            Assert.assertEquals(expectStartWebappCct, mDelegate.startWebappCctIntentCalled);
            Assert.assertEquals(expectStartActivity, startActivityCalled);
            Assert.assertEquals(expectStartWebApk, startWebApkCalled);
            Assert.assertEquals(expectStartFile, mDelegate.startFileIntentCalled);
            Assert.assertEquals(expectProxyForIA, mDelegate.mCalledWithProxy);

            if (startActivityCalled && expectSaneIntent) {
                checkIntentSanity(mDelegate.startActivityIntent, "Intent");
                if (mDelegate.startActivityIntent.getSelector() != null) {
                    checkIntentSanity(mDelegate.startActivityIntent.getSelector(),
                            "Intent's selector");
                }
            }
        }
    }

    private static String getPackageName() {
        return ContextUtils.getApplicationContext().getPackageName();
    }

    private static class TestPackageManager extends MockPackageManager {
        private ExternalNavigationDelegate mDelegate;

        public TestPackageManager(ExternalNavigationDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            return mDelegate.queryIntentActivities(intent);
        }
    }

    private static class TestContext extends ContextWrapper {
        private PackageManager mPackageManager;

        public TestContext(Context baseContext, ExternalNavigationDelegate delegate) {
            super(baseContext);
            mPackageManager = new TestPackageManager(delegate);
        }

        @Override
        public Context getApplicationContext() {
            return this;
        }

        @Override
        public PackageManager getPackageManager() {
            return mPackageManager;
        }

        @Override
        public String getPackageName() {
            return "test.app.name";
        }

        @Override
        public void startActivities(Intent[] intents, Bundle options) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void startActivity(Intent intent) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void startActivity(Intent intent, Bundle options) {
            throw new UnsupportedOperationException();
        }
    }
}
