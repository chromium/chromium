// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.MatcherAssert.assertThat;

import android.util.Pair;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.TestFetchStorageCallback;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Arrays;

/** Integration tests for {@link ServiceTabLauncher}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public final class ServiceTabLauncherTest {
    private static final long CHROME_LAUNCH_TIMEOUT_MS = 10000L;
    private static final String INITIATOR_URL = "https://initiator.example.com";
    private static final String PAGE_BODY = "<html><body>ok</body></html>";

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ServiceTabLauncher.Natives mServiceTabLauncherJni;

    private static TestWebServer sWebServer;

    @BeforeClass
    public static void setUpClass() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        sWebServer = TestWebServer.start();
    }

    @AfterClass
    public static void tearDownClass() {
        sWebServer.shutdown();
    }

    @Before
    public void setUp() {
        ServiceTabLauncherJni.setInstanceForTesting(mServiceTabLauncherJni);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> WebappRegistry.getInstance().clearForTesting());
    }

    private void registerWebappWithScope(String webappId, String scope) throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebappRegistry.getInstance().register(webappId, callback);
                });
        callback.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    callback.getStorage().updateScopeForTests(scope);
                });
    }

    private ChromeTabbedActivity launchTabAndWaitForActivity(
            String url, Origin initiatorOrigin, boolean isRendererInitiated) {
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                ServiceTabLauncher.launchTab(
                                        0,
                                        false,
                                        new GURL(url),
                                        WindowOpenDisposition.NEW_FOREGROUND_TAB,
                                        "",
                                        ReferrerPolicy.DEFAULT,
                                        "",
                                        null,
                                        initiatorOrigin,
                                        isRendererInitiated));
        mActivityTestRule.getActivityTestRule().setActivity(activity);
        return activity;
    }

    private void waitForRequest(String path) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(sWebServer.getRequestCount(path), Matchers.greaterThan(0));
                },
                CHROME_LAUNCH_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForTabLoaded(ChromeTabbedActivity activity, String url) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getActivityTab(), Matchers.notNullValue());
                },
                CHROME_LAUNCH_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
    }

    /**
     * Tests that the initiator origin passed to {@link ServiceTabLauncher#launchTab} is applied to
     * the navigation, so that the request carries fetch metadata describing its cross-site
     * initiator rather than appearing to be browser-initiated.
     */
    @Test
    @MediumTest
    public void testLaunchTabAttachesInitiatorOriginToNavigation() {
        String url = sWebServer.setResponse("/page.html", PAGE_BODY, null);
        Origin initiatorOrigin = Origin.create(new GURL(INITIATOR_URL));

        launchTabAndWaitForActivity(url, initiatorOrigin, true);
        waitForRequest("/page.html");

        Assert.assertEquals(
                "cross-site",
                sWebServer.getLastRequest("/page.html").headerValue("Sec-Fetch-Site"));
    }

    /** Tests that a launch without an initiator origin is still treated as browser-initiated. */
    @Test
    @MediumTest
    public void testLaunchTabWithoutInitiatorOriginIsBrowserInitiated() {
        String url = sWebServer.setResponse("/page.html", PAGE_BODY, null);

        launchTabAndWaitForActivity(url, null, false);
        waitForRequest("/page.html");

        Assert.assertEquals(
                "none", sWebServer.getLastRequest("/page.html").headerValue("Sec-Fetch-Site"));
    }

    /**
     * Tests that a launch on behalf of a cross-site initiator uses the initiator's cookie context:
     * cookies marked SameSite=Strict must not be attached to the request, while SameSite=Lax
     * cookies are attached as for any top-level cross-site navigation.
     */
    @Test
    @MediumTest
    public void testLaunchTabWithCrossSiteInitiatorExcludesSameSiteStrictCookies() {
        String setCookiesUrl =
                sWebServer.setResponse(
                        "/set-cookies.html",
                        PAGE_BODY,
                        Arrays.asList(
                                Pair.create(
                                        "Set-Cookie", "strict_cookie=1; SameSite=Strict; Path=/"),
                                Pair.create("Set-Cookie", "lax_cookie=1; SameSite=Lax; Path=/")));
        String targetUrl = sWebServer.setResponse("/target.html", PAGE_BODY, null);

        // Plant the cookies with a browser-initiated launch.
        ChromeTabbedActivity activity = launchTabAndWaitForActivity(setCookiesUrl, null, false);
        waitForTabLoaded(activity, setCookiesUrl);

        Origin initiatorOrigin = Origin.create(new GURL(INITIATOR_URL));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ServiceTabLauncher.launchTab(
                                0,
                                false,
                                new GURL(targetUrl),
                                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                                "",
                                ReferrerPolicy.DEFAULT,
                                "",
                                null,
                                initiatorOrigin,
                                true));
        waitForRequest("/target.html");

        String cookieHeader = sWebServer.getLastRequest("/target.html").headerValue("Cookie");
        assertThat(cookieHeader, Matchers.containsString("lax_cookie=1"));
        assertThat(cookieHeader, Matchers.not(Matchers.containsString("strict_cookie=1")));
    }

    /**
     * Tests that a cross-site launch targeting a URL that has recently-used standalone web app
     * storage is not captured into a standalone frame without initiator information, but opens as a
     * standard tab that excludes SameSite=Strict cookies.
     */
    @Test
    @MediumTest
    public void testLaunchTabWithCrossSiteInitiatorWhenWebappStorageExists() throws Exception {
        String setCookiesUrl =
                sWebServer.setResponse(
                        "/set-cookies.html",
                        PAGE_BODY,
                        Arrays.asList(
                                Pair.create(
                                        "Set-Cookie", "strict_cookie=1; SameSite=Strict; Path=/"),
                                Pair.create("Set-Cookie", "lax_cookie=1; SameSite=Lax; Path=/")));
        String targetUrl = sWebServer.setResponse("/target.html", PAGE_BODY, null);

        // Plant the cookies with a browser-initiated launch.
        ChromeTabbedActivity activity = launchTabAndWaitForActivity(setCookiesUrl, null, false);
        waitForTabLoaded(activity, setCookiesUrl);

        // Register WebappDataStorage for the target URL and mark it recently used.
        registerWebappWithScope("test_webapp_id", sWebServer.getBaseUrl());

        Origin initiatorOrigin = Origin.create(new GURL(INITIATOR_URL));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ServiceTabLauncher.launchTab(
                                0,
                                false,
                                new GURL(targetUrl),
                                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                                "",
                                ReferrerPolicy.DEFAULT,
                                "",
                                null,
                                initiatorOrigin,
                                true));
        waitForRequest("/target.html");

        String cookieHeader = sWebServer.getLastRequest("/target.html").headerValue("Cookie");
        assertThat(cookieHeader, Matchers.containsString("lax_cookie=1"));
        assertThat(cookieHeader, Matchers.not(Matchers.containsString("strict_cookie=1")));
    }

    /**
     * Tests that a renderer-initiated launch without an initiator origin is not trusted to capture
     * into a standalone web app frame, but falls through to a standard browser tab.
     */
    @Test
    @MediumTest
    public void testLaunchTabRendererInitiatedWithoutInitiatorDoesNotCaptureWebapp()
            throws Exception {
        String url = sWebServer.setResponse("/page.html", PAGE_BODY, null);

        // Register WebappDataStorage for the target URL and mark it recently used.
        registerWebappWithScope("test_webapp_id", sWebServer.getBaseUrl());

        launchTabAndWaitForActivity(url, null, true);
        waitForRequest("/page.html");

        Assert.assertEquals(1, sWebServer.getRequestCount("/page.html"));
        Assert.assertEquals(
                "cross-site",
                sWebServer.getLastRequest("/page.html").headerValue("Sec-Fetch-Site"));
    }

    /**
     * Tests that a renderer-initiated launch with an opaque initiator origin is not trusted to
     * capture into a standalone web app frame, but falls through to a standard browser tab.
     */
    @Test
    @MediumTest
    public void testLaunchTabRendererInitiatedWithOpaqueInitiatorDoesNotCaptureWebapp()
            throws Exception {
        String url = sWebServer.setResponse("/page.html", PAGE_BODY, null);

        // Register WebappDataStorage for the target URL and mark it recently used.
        registerWebappWithScope("test_webapp_id", sWebServer.getBaseUrl());

        Origin opaqueInitiator = Origin.createOpaqueOrigin();
        launchTabAndWaitForActivity(url, opaqueInitiator, true);
        waitForRequest("/page.html");

        Assert.assertEquals(1, sWebServer.getRequestCount("/page.html"));
        Assert.assertEquals(
                "cross-site",
                sWebServer.getLastRequest("/page.html").headerValue("Sec-Fetch-Site"));
    }

    /**
     * Tests {@link ServiceTabLauncher#canLaunchInApp} gating logic across browser-initiated,
     * same-origin, cross-origin, and opaque navigations.
     */
    @Test
    @SmallTest
    public void testCanLaunchInApp() {
        GURL targetUrl = new GURL("https://example.com/app/index.html");
        Origin sameOrigin = Origin.create(new GURL("https://example.com"));
        Origin crossOrigin = Origin.create(new GURL("https://attacker.com"));
        Origin differentPort = Origin.create(new GURL("https://example.com:8443"));
        Origin differentScheme = Origin.create(new GURL("http://example.com"));
        Origin opaqueOrigin = Origin.createOpaqueOrigin();

        // Browser-initiated navigations are always allowed to capture into apps.
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(false, null, targetUrl));
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(false, crossOrigin, targetUrl));

        // Renderer-initiated navigations require a non-null, non-opaque, strictly same-origin
        // initiator.
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, null, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, opaqueOrigin, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, crossOrigin, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, differentPort, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, differentScheme, targetUrl));
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(true, sameOrigin, targetUrl));
    }
}
