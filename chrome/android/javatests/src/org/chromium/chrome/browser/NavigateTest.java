// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.core.IsEqual.equalTo;

import android.content.pm.ActivityInfo;
import android.util.Base64;
import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.net.URL;
import java.util.Locale;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Navigate in UrlBar tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigateTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTTPS_SCHEME = "https://";

    private OmniboxTestUtils mOmnibox;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), ServerCertificate.CERT_OK);
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    private void navigateAndObserve(final String url) throws Exception {
        new TabLoadObserver(mActivityTestRule.getActivity().getActivityTab()).fullyLoadUrl(url);

        // Note: Omnibox does not present the scheme.
        mOmnibox.checkText(equalTo(expectedLocation(url)), null);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab url wrong",
                            ChromeTabUtils.getUrlStringOnUiThread(
                                    mActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is(url));
                });
    }

    /**
     * Types the passed text in the omnibox to trigger a navigation. You can pass a URL or a search
     * term. This code triggers suggestions and prerendering; unless you are testing these features
     * specifically, you should use loadUrl() which is less prone to flakyness.
     *
     * @param url The URL to navigate to.
     * @param expectedTitle Title that the page is expected to have. Shouldn't be set if the page
     *     load causes a redirect.
     * @return the URL in the omnibox.
     */
    private String typeInOmniboxAndNavigate(final String url, final String expectedTitle)
            throws Exception {
        mOmnibox.requestFocus();
        mOmnibox.typeText(url, false);
        mOmnibox.checkSuggestionsShown();

        // Loads the url.
        TabLoadObserver observer =
                new TabLoadObserver(
                        mActivityTestRule.getActivity().getActivityTab(), expectedTitle, null);
        mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
        observer.assertLoaded();

        // The URL has been set before the page notification was broadcast, so it is safe to access.
        return mOmnibox.getText();
    }

    /**
     * @return the expected contents of the location bar after navigating to url.
     */
    private String expectedLocation(String url) {
        Assert.assertTrue("url was:" + url, url.startsWith(HTTPS_SCHEME));
        return url.replaceFirst(HTTPS_SCHEME, "");
    }

    /** Verify Selection on the Location Bar. */
    @Test
    @MediumTest
    @Feature({"Navigation", "Main"})
    public void testNavigate() throws Exception {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }

    @Test
    @Restriction(DeviceFormFactor.TABLET)
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateMany() throws Exception {
        final String[] urls =
                mTestServer.getURLs(
                        "/chrome/test/data/android/navigate/one.html",
                        "/chrome/test/data/android/navigate/two.html",
                        "/chrome/test/data/android/navigate/three.html");
        final String[] titles = {"One", "Two", "Three"};
        final int repeats = 3;

        for (int i = 0; i < repeats; i++) {
            for (int j = 0; j < urls.length; j++) {
                String result = typeInOmniboxAndNavigate(urls[j], titles[j]);
                Assert.assertEquals(expectedLocation(urls[j]), result);
            }
        }
    }

    /** Verify Selection on the Location Bar in Landscape Mode */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateLandscape() throws Exception {
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
        // Reset device orientation.
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    /** Verify New Tab Open and Navigate. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisabledTest(message = "https://crbug.com/346968609")
    public void testOpenAndNavigate() throws Exception {
        final String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        navigateAndObserve(url);

        final int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertEquals(
                "Wrong number of tabs",
                tabCount + 1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }

    /** Test Opening a link and verify that the desired page is loaded. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/339299609
    public void testOpenLink() throws Exception {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        navigateAndObserve(url1);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        DOMUtils.clickNode(tab.getWebContents(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        Assert.assertEquals(
                "Desired Link not open",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    /** Test 'Request Desktop Site' option properly affects UA client hints */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint"})
    // TODO(crbug.com/40612550) Remove switch when UA-CH-* launched.
    public void testRequestDesktopSiteClientHints() throws Exception {
        String url1 =
                mTestServer.getURL(
                        "/set-header?Accept-CH: sec-ch-ua-arch,sec-ch-ua-platform,sec-ch-ua-model");
        String url2 =
                mTestServer.getURL(
                        "/echoheader?sec-ch-ua-arch&sec-ch-ua-mobile&sec-ch-ua-model&sec-ch-ua-platform");
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        navigateAndObserve(url1);
        ChromeTabUtils.waitForTabPageLoaded(tab, url1);

        navigateAndObserve(url2);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabUtils.switchUserAgent(
                                tab, /* switchToDesktop= */ true, UseDesktopUserAgentCaller.OTHER));
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        // Note: |content| is JSON, hence lots of escaping.
        Assert.assertEquals(
                "Proper headers",
                "\"\\\"x86\\\"\\n" + "?0\\n" + "\\\"\\\"\\n" + "\\\"Linux\\\"\"",
                content);
    }

    /** Test 'Request Desktop Site' option properly affects UA client hints with Critical-CH */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint, CriticalClientHint"})
    // TODO(crbug.com/40612550) Remove switch when UA-CH-* launched.
    public void testRequestDesktopSiteCriticalClientHints() throws Exception {
        // TODO(crbug.com/40153192): Move EchoCriticalHeader request handler here when
        // implemented
        String url = mTestServer.getURL("/echocriticalheader");
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        navigateAndObserve(url);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabUtils.switchUserAgent(
                                tab, /* switchToDesktop= */ true, UseDesktopUserAgentCaller.OTHER));

        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        // Note: |content| is JSON, hence lots of escaping.
        Assert.assertEquals("Proper headers", "\"?0\\\"Linux\\\"\"", content);
    }

    /**
     * Test Opening a link and verify that TabObserver#onPageLoadStarted gives the old and new URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/339299609
    public void testTabObserverOnPageLoadStarted() throws Exception {
        final String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        final String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        navigateAndObserve(url1);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        TabObserver onPageLoadStartedObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadStarted(Tab tab, GURL newUrl) {
                        tab.removeObserver(this);
                        Assert.assertEquals(url1, ChromeTabUtils.getUrlStringOnUiThread(tab));
                        Assert.assertEquals(url2, newUrl.getSpec());
                    }
                };
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(onPageLoadStartedObserver));
        DOMUtils.clickNode(tab.getWebContents(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        Assert.assertEquals(
                "Desired Link not open",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }

    /** Test re-direct functionality for a web-page. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateRedirect() throws Exception {
        final String initialUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/about.html");
        final String redirectedUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/one.html");
        typeInOmniboxAndNavigate(initialUrl, null);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(
                                    mActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is(redirectedUrl));
                });
    }

    /**
     * Test fallback works as intended and that we can go back to the original URL even when
     * redirected via Java redirection.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testIntentFallbackRedirection() throws Exception {
        final String fallbackUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/about.html");
        final String redirectUrl =
                "intent://non_existent/#Intent;scheme=non_existent;"
                        + "S.browser_fallback_url="
                        + fallbackUrl
                        + ";end";
        final String initialUrl =
                mTestServer.getURL(
                        "/chrome/test/data/android/redirect/js_redirect.html"
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(redirectUrl),
                                        Base64.URL_SAFE));
        final String targetUrl = mTestServer.getURL("/chrome/test/data/android/redirect/one.html");

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // We should start on the homepage, which is something other than our test page.
        String originalUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab());
        Criteria.checkThat(originalUrl, Matchers.not(targetUrl));

        typeInOmniboxAndNavigate(initialUrl, null);

        // Now intent fallback should be triggered assuming 'non_existent' scheme cannot be handled.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(
                                    mActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is(targetUrl));
                });

        // Check if Java redirections were removed from the history.
        // Note that if we try to go back in the test: NavigateToEntry() is called, but
        // DidNavigate() does not get called. But in real cases we can go back to initial page
        // without any problem.
        // TODO(changwan): figure out why we cannot go back on this test.
        int index =
                mActivityTestRule
                        .getActivity()
                        .getActivityTab()
                        .getWebContents()
                        .getNavigationController()
                        .getLastCommittedEntryIndex();
        Assert.assertEquals(1, index);
        String previousNavigationUrl =
                mActivityTestRule
                        .getActivity()
                        .getActivityTab()
                        .getWebContents()
                        .getNavigationController()
                        .getEntryAtIndex(0)
                        .getUrl()
                        .getSpec();
        Assert.assertEquals(originalUrl, previousNavigationUrl);
    }

    /** Test navigating back. */
    @Test
    @Restriction(DeviceFormFactor.PHONE)
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateBack() throws Exception {
        final String[] urls = {
            mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            navigateAndObserve(url);
        }

        final int repeats = 3;
        final ToolbarManager toolbarManager = mActivityTestRule.getActivity().getToolbarManager();

        for (int i = 0; i < repeats; i++) {
            Assert.assertNull(
                    "Back button is invisible in phone toolbar",
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            Assert.assertEquals(
                    "Tab should be able to be navigated back",
                    Boolean.TRUE,
                    toolbarManager.getHandleBackPressChangedSupplier().get());
            Assert.assertTrue(
                    "Tab has been navigated back",
                    ThreadUtils.runOnUiThreadBlocking(toolbarManager::back));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        }
        Assert.assertEquals(
                "Tab should be unable to be navigated back",
                Boolean.FALSE,
                toolbarManager.getHandleBackPressChangedSupplier().get());
        Assert.assertNull(
                "Back button is invisible in phone toolbar",
                mActivityTestRule.getActivity().findViewById(R.id.back_button));
    }

    /** Test back and forward buttons. */
    @Test
    @Restriction(DeviceFormFactor.TABLET)
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateBackAndForwardButtons() throws Exception {
        final String[] urls = {
            mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            navigateAndObserve(url);
        }

        final int repeats = 3;
        final ToolbarManager toolbarManager = mActivityTestRule.getActivity().getToolbarManager();
        for (int i = 0; i < repeats; i++) {
            onView(withId(R.id.back_button)).check(matches(isEnabled()));
            Assert.assertEquals(
                    "Tab should be able to be navigated back",
                    Boolean.TRUE,
                    toolbarManager.getHandleBackPressChangedSupplier().get());
            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(
                            Locale.US,
                            "URL mismatch after pressing back button for the 1st time in repetition"
                                    + "%d.",
                            i),
                    urls[1],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab()));

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(
                            Locale.US,
                            "URL mismatch after pressing back button for the 2nd time in repetition"
                                    + "%d.",
                            i),
                    urls[0],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab()));

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(
                            Locale.US,
                            "URL mismatch after pressing fwd button for the 1st time in repetition"
                                    + "%d.",
                            i),
                    urls[1],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab()));

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(
                            Locale.US,
                            "URL mismatch after pressing fwd button for the 2nd time in repetition"
                                    + "%d.",
                            i),
                    urls[2],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivityTestRule.getActivity().getActivityTab()));
        }

        for (int i = 0; i < repeats; i++) {
            onView(withId(R.id.back_button)).check(matches(isEnabled()));
            Assert.assertEquals(
                    "Tab should be able to be navigated back",
                    Boolean.TRUE,
                    toolbarManager.getHandleBackPressChangedSupplier().get());
            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        }
        Assert.assertEquals(
                "Tab should be unable to be navigated back",
                Boolean.FALSE,
                toolbarManager.getHandleBackPressChangedSupplier().get());
        onView(withId(R.id.back_button)).check(matches(Matchers.not(isEnabled())));
    }

    /** Test back with tab switcher. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisabledTest(message = "https://crbug.com/1410635")
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testNavigateBackWithTabSwitcher() throws Exception {
        final String[] urls = {
            mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
            mTestServer.getURL("/chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            navigateAndObserve(url);
        }

        String histogram = BackPressManager.getHistogramForTesting();

        HistogramWatcher startSurfaceHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogram,
                        BackPressManager.getHistogramValue(BackPressHandler.Type.START_SURFACE));
        HistogramWatcher tabSwitcherHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogram,
                        BackPressManager.getHistogramValue(BackPressHandler.Type.TAB_SWITCHER));

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.enterTabSwitcher(cta);
        Assert.assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed();
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    int type =
                            mActivityTestRule
                                    .getActivity()
                                    .getLayoutManager()
                                    .getActiveLayoutType();
                    Assert.assertEquals(LayoutType.BROWSING, type);
                });

        try {
            startSurfaceHistogram.assertExpected();
        } catch (AssertionError e) {
            tabSwitcherHistogram.assertExpected(
                    "Either start surface or tab switcher handles back press.");
        }
    }

    /** Test back with tab switcher. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    @DisabledTest(message = "https://crbug.com/1410635")
    public void testNavigateBackWithTabSwitcher_BackPressRefactor() throws Exception {
        // Disable iph
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager backPressManager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    backPressManager.removeHandler(BackPressHandler.Type.TEXT_BUBBLE);
                });
        testNavigateBackWithTabSwitcher();
    }

    @Test
    @DisableIf.Build(hardware_is = "sprout", message = "fails on android-one: crbug.com/540723")
    @MediumTest
    @Feature({"Navigation"})
    public void testWindowOpenUrlSpoof() throws Exception {
        // TODO(jbudorick): Convert this from TestWebServer to EmbeddedTestServer.
        TestWebServer webServer = TestWebServer.start();
        try {
            // Make sure that we start with one tab.
            final TabModel model =
                    mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

            final Semaphore urlServedSemaphore = new Semaphore(0);
            Runnable checkAction =
                    () -> {
                        final Tab tab = TabModelUtils.getCurrentTab(model);

                        // Make sure that we are showing the spoofed data and a blank URL.
                        String url = getTabUrlOnUIThread(tab);
                        boolean spoofedUrl = "".equals(url) || "about:blank".equals(url);
                        Assert.assertTrue("URL Spoofed", spoofedUrl);
                        Assert.assertEquals(
                                "Not showing mocked content", "\"Spoofed\"", getTabBodyText(tab));
                        urlServedSemaphore.release();
                    };

            // Mock out the test URL
            final String mockedUrl =
                    webServer.setResponseWithRunnableAction(
                            "/mockme.html",
                            "<html>  <head>    <meta name=\"viewport\"       "
                                + " content=\"initial-scale=0.75,maximum-scale=0.75,user-scalable=no\">"
                                + "  </head>  <body>Real</body></html>",
                            null,
                            checkAction);

            // Navigate to the spoofable URL
            mActivityTestRule.loadUrl(
                    UrlUtils.encodeHtmlDataUri(
                            "<head>  <meta name=\"viewport\"     "
                                + " content=\"initial-scale=0.5,maximum-scale=0.5,user-scalable=no\"></head><script>"
                                + "  function spoof() {    var w = open();    w.opener = null;   "
                                + " w.document.write('Spoofed');    w.location = '"
                                    + mockedUrl
                                    + "'"
                                    + "  }"
                                    + "</script>"
                                    + "<body id='body' onclick='spoof()'></body>"));
            mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

            // Click the page, which triggers the URL load.
            DOMUtils.clickNode(mActivityTestRule.getActivity().getCurrentWebContents(), "body");

            // Wait for the proper URL to be served.
            Assert.assertTrue(urlServedSemaphore.tryAcquire(5, TimeUnit.SECONDS));

            // Wait for the url to change.
            final Tab tab = TabModelUtils.getCurrentTab(model);
            mActivityTestRule.assertWaitForPageScaleFactorMatch(0.75f);
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        Criteria.checkThat(getTabUrlOnUIThread(tab), Matchers.is(mockedUrl));
                    },
                    5000,
                    50);

            // Make sure that we're showing new content now.
            Assert.assertEquals("Still showing spoofed data", "\"Real\"", getTabBodyText(tab));
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Test that if the browser launches a renderer initiated intent towards itself, the url load
     * will be renderer initiated and has the correct origin.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisabledTest(message = "crbug.com/1130419")
    public void testRendererInitiatedIntentNavigate() throws Exception {
        final String finalUrl =
                mTestServer.getURL("/chrome/test/data/android/renderer_initiated/final.html");
        // The launched intent will have the following format:
        // android-app://package_name/xx.xx.xx.xx:xxxx/testDirs/final.html.
        final String intentUrl =
                "android-app://"
                        + ContextUtils.getApplicationContext().getPackageName()
                        + "/"
                        + finalUrl.replace("://", "/");

        // The second page will launch the |intentUrl| to load |finalUrl|.
        final String secondUrl =
                mTestServer.getURL(
                        "/chrome/test/data/android/renderer_initiated/renderer_initiated.html?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("URL"), Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(intentUrl),
                                        Base64.URL_SAFE));

        // Passing |secondUrl| to the first page, so that clicking on the link will trigger the
        // renderer initiated intent.ss
        final String firstUrl =
                mTestServer.getURL(
                        "/chrome/test/data/android/renderer_initiated/first.html"
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(secondUrl),
                                        Base64.URL_SAFE));

        navigateAndObserve(firstUrl);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        TabObserver onPageLoadStartedObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onLoadUrl(
                            Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
                        tab.removeObserver(this);
                        // Check that the final URL will be loaded properly, and the navigation
                        // is renderer initiated and has the correct origin.
                        Assert.assertEquals(finalUrl, params.getUrl());
                        Assert.assertEquals(true, params.getIsRendererInitiated());
                        try {
                            URL url = new URL(finalUrl);
                            Origin origin = params.getInitiatorOrigin();
                            Assert.assertEquals(url.getHost(), origin.getHost());
                        } catch (Exception e) {
                            throw new AssertionError("Cannot parse URL:" + finalUrl, e);
                        }
                    }
                };
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(onPageLoadStartedObserver));
        DOMUtils.clickNode(tab.getWebContents(), "rendererInitiated");
        ChromeTabUtils.waitForTabPageLoaded(tab, finalUrl);
    }

    private String getTabUrlOnUIThread(final Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.getUrlStringOnUiThread(tab));
    }

    private String getTabBodyText(Tab tab) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    tab.getWebContents(), "document.body.innerText");
        } catch (Exception ex) {
            assert false : "Unexpected Exception";
        }
        return null;
    }
}
