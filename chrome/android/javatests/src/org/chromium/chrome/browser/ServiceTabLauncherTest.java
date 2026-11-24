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
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Triggers;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.webapps.TestFetchStorageCallback;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
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
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                ServiceTabLauncher.launchTab(
                                        0,
                                        profile,
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
        waitForRequest(sWebServer, path);
    }

    private void waitForRequest(TestWebServer webServer, String path) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(webServer.getRequestCount(path), Matchers.greaterThan(0));
                },
                CHROME_LAUNCH_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForTabLoaded(ChromeTabbedActivity activity, String url) {
        // The activity was just launched, so its tab may not exist yet.
        Triggers.noopTo()
                .withTimeout(CHROME_LAUNCH_TIMEOUT_MS)
                .hopOnTo(activity, WebPageStation.newBuilder().initForEntryPointUrl(url).build());
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
                                ProfileManager.getLastUsedRegularProfile(),
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
                                ProfileManager.getLastUsedRegularProfile(),
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
     * Tests that a launch requested by a non-primary off-the-record profile (such as an Ephemeral
     * or Incognito Custom Tab) is rejected and does not open a tab in the primary Incognito
     * profile.
     */
    @Test
    @MediumTest
    public void testLaunchTabRejectsNonPrimaryOtrProfile() {
        String initialUrl = sWebServer.setResponse("/initial.html", PAGE_BODY, null);
        String targetUrl = sWebServer.setResponse("/non-primary-otr.html", PAGE_BODY, null);

        ChromeTabbedActivity activity = launchTabAndWaitForActivity(initialUrl, null, false);
        waitForTabLoaded(activity, initialUrl);

        Origin initiatorOrigin = Origin.create(new GURL(INITIATOR_URL));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile regularProfile = ProfileManager.getLastUsedRegularProfile();
                    Profile ephemeralCctProfile =
                            regularProfile.getOrCreateOffTheRecordProfile(
                                    OtrProfileId.createUnique("CCT:Ephemeral"));
                    Profile incognitoCctProfile =
                            regularProfile.getOrCreateOffTheRecordProfile(
                                    OtrProfileId.createUniqueIncognitoCctId());

                    ServiceTabLauncher.launchTab(
                            42,
                            ephemeralCctProfile,
                            new GURL(targetUrl),
                            WindowOpenDisposition.NEW_FOREGROUND_TAB,
                            "",
                            ReferrerPolicy.DEFAULT,
                            "",
                            null,
                            initiatorOrigin,
                            true);
                    ServiceTabLauncher.launchTab(
                            43,
                            incognitoCctProfile,
                            new GURL(targetUrl),
                            WindowOpenDisposition.NEW_FOREGROUND_TAB,
                            "",
                            ReferrerPolicy.DEFAULT,
                            "",
                            null,
                            initiatorOrigin,
                            true);

                    ProfileManager.destroyWhenAppropriate(ephemeralCctProfile);
                    ProfileManager.destroyWhenAppropriate(incognitoCctProfile);
                });

        Mockito.verify(mServiceTabLauncherJni, Mockito.timeout(CHROME_LAUNCH_TIMEOUT_MS))
                .onWebContentsForRequestAvailable(42, null);
        Mockito.verify(mServiceTabLauncherJni, Mockito.timeout(CHROME_LAUNCH_TIMEOUT_MS))
                .onWebContentsForRequestAvailable(43, null);
        Assert.assertEquals(0, sWebServer.getRequestCount("/non-primary-otr.html"));
        int incognitoTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                activity.getTabModelSelector()
                                        .getModel(/* incognito= */ true)
                                        .getCount());
        Assert.assertEquals(0, incognitoTabCount);
    }

    /**
     * Tests that a launch requested by the primary Incognito profile for a URL matching a
     * recently-used standalone web app stays inside an Incognito browser tab rather than escaping
     * into a regular-profile web app frame with regular-profile cookies.
     */
    @Test
    @MediumTest
    public void testLaunchTabPrimaryIncognitoDoesNotCaptureWebapp() throws Exception {
        String setCookiesUrl =
                sWebServer.setResponse(
                        "/set-cookies.html",
                        PAGE_BODY,
                        Arrays.asList(Pair.create("Set-Cookie", "regular_cookie=1; Path=/")));

        // Plant a host-only localhost cookie in the regular profile first, then serve the target
        // page from an additional TestWebServer instance so an unused regular-profile preconnect
        // socket cannot block single-threaded WebServer.ServerThread while the isolated Incognito
        // socket pool connects.
        ChromeTabbedActivity activity = launchTabAndWaitForActivity(setCookiesUrl, null, false);
        waitForTabLoaded(activity, setCookiesUrl);

        try (TestWebServer targetServer = TestWebServer.startAdditional()) {
            String targetUrl = targetServer.setResponse("/incognito-target.html", PAGE_BODY, null);
            registerWebappWithScope("test_webapp_id", targetServer.getBaseUrl());

            int initialIncognitoTabCount =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> TabWindowManagerSingleton.getInstance().getIncognitoTabCount());
            Assert.assertEquals(0, initialIncognitoTabCount);

            Origin sameOriginInitiator = Origin.create(new GURL(targetUrl));
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Profile primaryIncognitoProfile =
                                ProfileManager.getLastUsedRegularProfile()
                                        .getOrCreatePrimaryOtrProfile();
                        ServiceTabLauncher.launchTab(
                                0,
                                primaryIncognitoProfile,
                                new GURL(targetUrl),
                                WindowOpenDisposition.NEW_FOREGROUND_TAB,
                                "",
                                ReferrerPolicy.DEFAULT,
                                "",
                                null,
                                sameOriginInitiator,
                                true);
                    });
            waitForRequest(targetServer, "/incognito-target.html");

            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                TabWindowManagerSingleton.getInstance().getIncognitoTabCount(),
                                Matchers.is(1));
                    },
                    CHROME_LAUNCH_TIMEOUT_MS,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            String cookieHeader =
                    targetServer.getLastRequest("/incognito-target.html").headerValue("Cookie");
            Assert.assertEquals("", cookieHeader);
        }
    }

    /**
     * Tests {@link ServiceTabLauncher#canLaunchInApp} gating logic across off-the-record,
     * browser-initiated, same-origin, cross-origin, and opaque navigations.
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

        // Off-the-record navigations must never capture into regular-profile web apps.
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, false, null, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(true, true, sameOrigin, targetUrl));

        // Browser-initiated regular-profile navigations are allowed to capture into apps.
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(false, false, null, targetUrl));
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(false, false, crossOrigin, targetUrl));

        // Renderer-initiated navigations require a non-null, non-opaque, strictly same-origin
        // initiator.
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(false, true, null, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(false, true, opaqueOrigin, targetUrl));
        Assert.assertFalse(ServiceTabLauncher.canLaunchInApp(false, true, crossOrigin, targetUrl));
        Assert.assertFalse(
                ServiceTabLauncher.canLaunchInApp(false, true, differentPort, targetUrl));
        Assert.assertFalse(
                ServiceTabLauncher.canLaunchInApp(false, true, differentScheme, targetUrl));
        Assert.assertTrue(ServiceTabLauncher.canLaunchInApp(false, true, sameOrigin, targetUrl));
    }
}
