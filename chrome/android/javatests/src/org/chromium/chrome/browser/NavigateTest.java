// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.util.Base64;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Navigate in UrlBar tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigateTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTTP_SCHEME = "http://";
    private static final String NEW_TAB_PAGE = "chrome-native://newtab/";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void navigateAndObserve(final String startUrl, final String endUrl)
            throws Exception {
        new TabLoadObserver(mActivityTestRule.getActivity().getActivityTab())
                .fullyLoadUrl(startUrl);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                final UrlBar urlBar =
                        (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
                Assert.assertNotNull("urlBar is null", urlBar);

                if (!TextUtils.equals(expectedLocation(endUrl), urlBar.getText().toString())) {
                    updateFailureReason(String.format("Expected url bar text: %s, actual: %s",
                            expectedLocation(endUrl), urlBar.getText().toString()));
                    return false;
                }
                if (!TextUtils.equals(
                            endUrl, mActivityTestRule.getActivity().getActivityTab().getUrl())) {
                    updateFailureReason(String.format("Expected tab url: %s, actual: %s", endUrl,
                            mActivityTestRule.getActivity().getActivityTab().getUrl()));
                    return false;
                }
                return true;
            }
        });
    }

    /**
     * Types the passed text in the omnibox to trigger a navigation. You can pass a URL or a search
     * term. This code triggers suggestions and prerendering; unless you are testing these
     * features specifically, you should use loadUrl() which is less prone to flakyness.
     *
     * @param url The URL to navigate to.
     * @param expectedTitle Title that the page is expected to have.  Shouldn't be set if the page
     *                      load causes a redirect.
     * @return the URL in the omnibox.
     */
    private String typeInOmniboxAndNavigate(final String url, final String expectedTitle)
            throws Exception {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull("urlBar is null", urlBar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            urlBar.requestFocus();
            urlBar.setText(url);
        });
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        // Loads the url.
        TabLoadObserver observer = new TabLoadObserver(
                mActivityTestRule.getActivity().getActivityTab(), expectedTitle, null);
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);
        observer.assertLoaded();

        // The URL has been set before the page notification was broadcast, so it is safe to access.
        return urlBar.getText().toString();
    }

    /**
     * @return the expected contents of the location bar after navigating to url.
     */
    private String expectedLocation(String url) {
        Assert.assertTrue(url.startsWith(HTTP_SCHEME));
        return url.replaceFirst(HTTP_SCHEME, "");
    }

    /**
     * Verify Selection on the Location Bar.
     */
    @Test
    @MediumTest
    @Feature({"Navigation", "Main"})
    @RetryOnFailure
    public void testNavigate() throws Exception {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @MediumTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testNavigateMany() throws Exception {
        final String[] urls = mTestServer.getURLs("/chrome/test/data/android/navigate/one.html",
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

    /**
     * Verify Selection on the Location Bar in Landscape Mode
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testNavigateLandscape() throws Exception {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }

    /**
     * Verify New Tab Open and Navigate.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testOpenAndNavigate() throws Exception {
        final String url =
                mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        navigateAndObserve(url, url);

        final int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertEquals("Wrong number of tabs", tabCount + 1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }

    /**
     * Test Opening a link and verify that the desired page is loaded.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testOpenLink() throws Exception {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        navigateAndObserve(url1, url1);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        DOMUtils.clickNode(tab.getWebContents(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        Assert.assertEquals("Desired Link not open", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Test 'Request Desktop Site' option is preserved after navigation to a new entry
     * through a click on a link.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @DisabledTest(message = "crbug.com/879153")
    @RetryOnFailure
    public void testRequestDesktopSiteSettingPers() throws Exception {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        navigateAndObserve(url1, url1);

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                                true /* useDesktop */, true /* reloadOnChange */));
        ChromeTabUtils.waitForTabPageLoaded(tab, url1);

        DOMUtils.clickNode(tab.getWebContents(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        Assert.assertEquals("Request Desktop site setting should stay turned on", true,
                mActivityTestRule.getActivity()
                        .getActivityTab()
                        .getWebContents()
                        .getNavigationController()
                        .getUseDesktopUserAgent());
    }

    /**
     * Test Opening a link and verify that TabObserver#onPageLoadStarted gives the old and new URL.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    @RetryOnFailure
    public void testTabObserverOnPageLoadStarted() throws Exception {
        final String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        final String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        navigateAndObserve(url1, url1);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        TabObserver onPageLoadStartedObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String newUrl) {
                tab.removeObserver(this);
                Assert.assertEquals(url1, tab.getUrl());
                Assert.assertEquals(url2, newUrl);
            }
        };
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        tab.addObserver(onPageLoadStartedObserver);
        DOMUtils.clickNode(tab.getWebContents(), "aboutLink");
        ChromeTabUtils.waitForTabPageLoaded(tab, url2);
        Assert.assertEquals("Desired Link not open", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Test re-direct functionality for a web-page.
     * @throws Exception
     */
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
                Criteria.equals(redirectedUrl,
                        () -> mActivityTestRule.getActivity().getActivityTab().getUrl()));
    }

    /**
     * Test fallback works as intended and that we can go back to the original URL
     * even when redirected via Java redirection.
     */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testIntentFallbackRedirection() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                NEW_TAB_PAGE, mActivityTestRule.getActivity().getActivityTab().getUrl());

        final String fallbackUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/about.html");
        final String redirectUrl = "intent://non_existent/#Intent;scheme=non_existent;"
                + "S.browser_fallback_url=" + fallbackUrl + ";end";
        final String initialUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/js_redirect.html"
                        + "?replace_text="
                        + Base64.encodeToString(
                                  ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"), Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(ApiCompatibilityUtils.getBytesUtf8(redirectUrl),
                                  Base64.URL_SAFE));
        final String targetUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/one.html");
        typeInOmniboxAndNavigate(initialUrl, null);

        // Now intent fallback should be triggered assuming 'non_existent' scheme cannot be handled.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(targetUrl,
                () -> mActivityTestRule.getActivity().getActivityTab().getUrl()));

        // Check if Java redirections were removed from the history.
        // Note that if we try to go back in the test: NavigateToEntry() is called, but
        // DidNavigate() does not get called. But in real cases we can go back to initial page
        // without any problem.
        // TODO(changwan): figure out why we cannot go back on this test.
        int index = mActivityTestRule.getActivity()
                            .getActivityTab()
                            .getWebContents()
                            .getNavigationController()
                            .getLastCommittedEntryIndex();
        Assert.assertEquals(1, index);
        String previousNavigationUrl = mActivityTestRule.getActivity()
                                               .getActivityTab()
                                               .getWebContents()
                                               .getNavigationController()
                                               .getEntryAtIndex(0)
                                               .getUrl();
        Assert.assertEquals(NEW_TAB_PAGE, previousNavigationUrl);
    }

    /**
     * Test back and forward buttons.
     */
    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @MediumTest
    @Feature({"Navigation"})
    public void testNavigateBackAndForwardButtons() throws Exception {
        final String[] urls = {
                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            navigateAndObserve(url, url);
        }

        final int repeats = 3;
        for (int i = 0; i < repeats; i++) {
            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(Locale.US,
                            "URL mismatch after pressing back button for the 1st time in repetition"
                                    + "%d.",
                            i),
                    urls[1], mActivityTestRule.getActivity().getActivityTab().getUrl());

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.back_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(Locale.US,
                            "URL mismatch after pressing back button for the 2nd time in repetition"
                                    + "%d.",
                            i),
                    urls[0], mActivityTestRule.getActivity().getActivityTab().getUrl());

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(Locale.US,
                            "URL mismatch after pressing fwd button for the 1st time in repetition"
                                    + "%d.",
                            i),
                    urls[1], mActivityTestRule.getActivity().getActivityTab().getUrl());

            TouchCommon.singleClickView(
                    mActivityTestRule.getActivity().findViewById(R.id.forward_button));
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
            Assert.assertEquals(
                    String.format(Locale.US,
                            "URL mismatch after pressing fwd button for the 2nd time in repetition"
                                    + "%d.",
                            i),
                    urls[2], mActivityTestRule.getActivity().getActivityTab().getUrl());
        }
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
            Runnable checkAction = () -> {
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
            final String mockedUrl = webServer.setResponseWithRunnableAction("/mockme.html",
                    "<html>"
                    + "  <head>"
                    + "    <meta name=\"viewport\""
                    + "        content=\"initial-scale=0.75,maximum-scale=0.75,user-scalable=no\">"
                    + "  </head>"
                    + "  <body>Real</body>"
                    + "</html>", null, checkAction);

            // Navigate to the spoofable URL
            mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("<head>"
                    + "  <meta name=\"viewport\""
                    + "      content=\"initial-scale=0.5,maximum-scale=0.5,user-scalable=no\">"
                    + "</head>"
                    + "<script>"
                    + "  function spoof() {"
                    + "    var w = open();"
                    + "    w.opener = null;"
                    + "    w.document.write('Spoofed');"
                    + "    w.location = '" + mockedUrl + "'"
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
                    Criteria.equals(mockedUrl, () -> getTabUrlOnUIThread(tab)), 5000, 50);

            // Make sure that we're showing new content now.
            Assert.assertEquals("Still showing spoofed data", "\"Real\"", getTabBodyText(tab));
        } finally {
            webServer.shutdown();
        }
    }

    private String getTabUrlOnUIThread(final Tab tab) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> tab.getUrl());
        } catch (ExecutionException ex) {
            assert false : "Unexpected ExecutionException";
        }
        return null;
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
