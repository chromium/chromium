// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.content_settings.PrefNames.IN_CONTEXT_COOKIE_CONTROLS_OPENED;
import static org.chromium.ui.test.util.ViewUtils.hasBackgroundColor;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.text.format.DateUtils;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.FederatedIdentityTestUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryContentManager;
import org.chromium.chrome.browser.history.StubbedHistoryProvider;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfPageType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoAdPersonalizationController;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.GURLUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Random;
import java.util.concurrent.TimeoutException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different configurations
 * correctly.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
// TODO(crbug.com/344672095): Failing when batched, batch this again.
// Disable TrackingProtection3pcd as we use prefs instead of the feature in
// these tests.
@DisableFeatures({
    ChromeFeatureList.TRACKING_PROTECTION_3PCD,
    ChromeFeatureList.TRACKING_PROTECTION_3PCD_UX
})
public class PageInfoViewTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";
    private static final String sSiteDataHtml = "/content/test/data/browsing_data/site_data.html";

    private static String[] sCookieDataTypes = {
        "Cookie", "LocalStorage", "ServiceWorker", "CacheStorage", "IndexedDb", "FileSystem"
    };

    // June 4, 2021 12:00:00 GMT+00:00
    private static long sTimestampJune4 = 1622808000000L;
    // April 4, 2021 12:00:00 GMT+00:00
    private static long sTimestampApril4 = 1617537600000L;

    /**
     * Parameter provider for testing the different timestamps for the history section's "last
     * visited" text.
     */
    public static class HistorySummaryTestParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            Resources res = ApplicationProvider.getApplicationContext().getResources();
            Random random = new Random();
            long timestamp;

            List<ParameterSet> parameters = new ArrayList<>();
            // ParameterSet = {timestamp, string}
            timestamp = CalendarUtils.getStartOfDay(sTimestampJune4).getTime().getTime();
            parameters.add(
                    new ParameterSet()
                            .value(
                                    timestamp,
                                    res.getString(R.string.page_info_history_last_visit_today))
                            .name("Today"));
            timestamp = sTimestampJune4 - 1 * DateUtils.DAY_IN_MILLIS;
            parameters.add(
                    new ParameterSet()
                            .value(
                                    timestamp,
                                    res.getString(R.string.page_info_history_last_visit_yesterday))
                            .name("Yesterday"));
            int offset = random.nextInt(6) + 2;
            timestamp = sTimestampJune4 - offset * DateUtils.DAY_IN_MILLIS;
            parameters.add(
                    new ParameterSet()
                            .value(
                                    timestamp,
                                    res.getString(
                                            R.string.page_info_history_last_visit_days, offset))
                            .name("XDaysAgo"));
            parameters.add(
                    new ParameterSet()
                            .value(
                                    sTimestampApril4,
                                    res.getString(
                                            R.string.page_info_history_last_visit_date,
                                            StringUtils.dateToHeaderString(
                                                    new Date(sTimestampApril4))))
                            .name("ExactDay"));
            return parameters;
        }
    }

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(8)
                    .setDescription("Red interstitial color, icon, and string facelift")
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_BUBBLES_PAGE_INFO)
                    .build();

    private boolean mIsSystemLocationSettingEnabled = true;

    private class TestLocationUtils extends LocationUtils {
        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mIsSystemLocationSettingEnabled;
        }
    }

    private void loadUrlAndOpenPageInfo(String url) {
        loadUrlAndOpenPageInfoWithPermission(url, PageInfoController.NO_HIGHLIGHTED_PERMISSION);
    }

    private void loadUrlAndOpenPageInfoWithPermission(
            String url, @ContentSettingsType.EnumType int highlightedPermission) {
        sActivityTestRule.loadUrl(url);
        openPageInfo(highlightedPermission);
    }

    private void openPageInfo(@ContentSettingsType.EnumType int highlightedPermission) {
        ChromeActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new ChromePageInfo(
                                    activity.getModalDialogManagerSupplier(),
                                    null,
                                    PageInfoController.OpenedFromSource.TOOLBAR,
                                    null,
                                    null,
                                    null)
                            .show(
                                    tab,
                                    ChromePageInfoHighlight.forPermission(highlightedPermission));
                });
        onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()), true);
    }

    private View getPageInfoView() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        View view = controller.getPageInfoViewForTesting();
        assertNotNull(view);
        return view;
    }

    private void enableTrackingProtectionFixedExpiration() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.IP_PROTECTION_USER_BYPASS)
                || ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FINGERPRINTING_PROTECTION_USER_BYPASS)) {
            var tpController = controller.getTrackingProtectionLaunchControllerForTesting();
            tpController.setFixedExceptionExpirationForTesting(true);
        } else {
            var tpController = controller.getTrackingProtectionControllerForTesting();
            tpController.setFixedExceptionExpirationForTesting(true);
        }
    }

    private void setThirdPartyCookieBlocking(@CookieControlsMode int value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(COOKIE_CONTROLS_MODE, value);
                });
    }

    private void enableTrackingProtection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED, true);
                });
    }

    private void setBlockAll3PC(boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED, value);
                });
    }

    private String runJavascriptAsync(String type) throws TimeoutException {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                sActivityTestRule.getWebContents(), type);
    }

    private void expectHasCookies(boolean hasData) throws TimeoutException {
        for (String type : sCookieDataTypes) {
            assertEquals(hasData ? "true" : "false", runJavascriptAsync("has" + type + "Async()"));
        }
    }

    private void createCookies() throws TimeoutException {
        for (String type : sCookieDataTypes) {
            runJavascriptAsync("set" + type + "Async()");
        }
    }

    private void addSomePermissions(String urlString) {
        GURL url = new GURL(urlString);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.GEOLOCATION,
                            url,
                            url,
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.NOTIFICATIONS,
                            url,
                            url,
                            ContentSettingValues.BLOCK);
                });
    }

    private void expectHasPermissions(String url, boolean hasPermissions) {
        // The default value for these types is ask.
        @ContentSettingValues
        int expectAllow = hasPermissions ? ContentSettingValues.ALLOW : ContentSettingValues.ASK;
        @ContentSettingValues
        int expectBlock = hasPermissions ? ContentSettingValues.BLOCK : ContentSettingValues.ASK;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            expectBlock,
                            WebsitePreferenceBridgeJni.get()
                                    .getPermissionSettingForOrigin(
                                            ProfileManager.getLastUsedRegularProfile(),
                                            ContentSettingsType.NOTIFICATIONS,
                                            url,
                                            url));
                    assertEquals(
                            expectAllow,
                            WebsitePreferenceBridgeJni.get()
                                    .getPermissionSettingForOrigin(
                                            ProfileManager.getLastUsedRegularProfile(),
                                            ContentSettingsType.GEOLOCATION,
                                            url,
                                            "*"));
                });
    }

    private void addDefaultSettingPermissions(String urlString) {
        GURL url = new GURL(urlString);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.MEDIASTREAM_MIC,
                            url,
                            url,
                            ContentSettingValues.DEFAULT);
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.MEDIASTREAM_CAMERA,
                            url,
                            url,
                            ContentSettingValues.ASK);
                });
    }

    private void clearPermissions() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    private List<ContentSettingException> getNonWildcardContentSettingExceptions(
            @ContentSettingsType.EnumType int type) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<ContentSettingException> exceptions =
                            new ArrayList<ContentSettingException>();
                    WebsitePreferenceBridgeJni.get()
                            .getContentSettingsExceptions(
                                    ProfileManager.getLastUsedRegularProfile(), type, exceptions);
                    Iterator<ContentSettingException> exceptionIt = exceptions.iterator();
                    while (exceptionIt.hasNext()) {
                        ContentSettingException exception = exceptionIt.next();
                        if (WebsitePreferenceBridge.SITE_WILDCARD.equals(
                                        exception.getPrimaryPattern())
                                && WebsitePreferenceBridge.SITE_WILDCARD.equals(
                                        exception.getSecondaryPattern())) {
                            exceptionIt.remove();
                        }
                    }
                    return exceptions;
                });
    }

    private void addSomeHistoryEntries() {
        StubbedHistoryProvider historyProvider = new StubbedHistoryProvider();
        // Need to always have the same dates for render tests.
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, sTimestampApril4));
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, sTimestampJune4));
        HistoryContentManager.setProviderForTests(historyProvider);
        PageInfoHistoryController.setProviderForTests(historyProvider);
    }

    @Before
    public void setUp() throws InterruptedException {
        // Some test devices have geolocation disabled. Override LocationUtils for a
        // stable result.
        LocationUtils.setFactory(TestLocationUtils::new);

        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);

        PageInfoAdPersonalizationController.setTopicsForTesting(Arrays.asList("Testing topic"));
    }

    @After
    public void tearDown() throws TimeoutException {
        LocationUtils.setFactory(null);
        // Notification channels don't get cleaned up automatically.
        // TODO(crbug.com/41452182): Find a general solution to avoid leaking channels
        // between
        // tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SiteChannelsManager manager = SiteChannelsManager.getInstance();
                        manager.deleteAllSiteChannels();
                    });
        }

        setThirdPartyCookieBlocking(CookieControlsMode.INCOGNITO_ONLY);
        clearPermissions();
    }

    /** Tests that PageInfoController converts safe URLs to Unicode. */
    @Test
    @MediumTest
    @Feature({"PageInfoController"})
    public void testPageInfoUrl() {
        String testUrl =
                mTestServerRule.getServer().getURLWithHostName("xn--allestrungen-9ib.ch", "/");
        loadUrlAndOpenPageInfo(testUrl);
        onView(
                withText(
                        allOf(
                                containsString("allestörungen.ch"),
                                not(containsString("https://")))));
        // Expand to full URL.
        onView(withId(R.id.page_info_url_wrapper)).perform(click());
        onView(withText(allOf(containsString("allestörungen.ch"), containsString("https://"))));
    }

    /** Tests PageInfo on an insecure website. */
    @Test
    @MediumTest
    public void testShowOnInsecureHttpWebsite() throws IOException {
        mTestServerRule.setServerUsesHttps(false);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onViewWaiting(allOf(withId(R.id.page_info_connection_row), isDisplayed()));
        onView(withText("Connection is not secure")).check(matches(isDisplayed()));
    }

    /** Tests PageInfo on a secure website. */
    @Test
    @MediumTest
    public void testShowOnSecureWebsite() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onViewWaiting(allOf(withId(R.id.page_info_connection_row), isDisplayed()));
        onView(withText("Connection is secure")).check(matches(isDisplayed()));
    }

    /** Tests PageInfo on a website with expired certificate. */
    @Test
    @MediumTest
    public void testShowOnExpiredCertificateWebsite() throws IOException {
        mTestServerRule.setCertificateType(ServerCertificate.CERT_EXPIRED);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onViewWaiting(allOf(withId(R.id.page_info_connection_row), isDisplayed()));
        onView(withText("Connection is not secure")).check(matches(isDisplayed()));
    }

    /** Tests PageInfo on internal page. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testChromePage() throws IOException {
        loadUrlAndOpenPageInfo("chrome://version/");
        mRenderTestRule.render(getPageInfoView(), "PageInfo_InternalSite");
    }

    /**
     * Tests PageInfo on a website with permissions. Geolocation is blocked system wide in this
     * test.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsTurnedOffForDevice() throws IOException {
        mIsSystemLocationSettingEnabled = false;
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsTurnedOffForDevice");
    }

    /** Tests PageInfo on a website with cookie controls and permissions. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsAndCookieBlocking() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions");
    }

    /**
     * Tests PageInfo on a website with cookie controls and permissions with User Bypass enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsAndCookieBlockingUserBypass() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions_UserBypass");
    }

    /** Tests PageInfo on a website with 3PC and permissions with Tracking Protection enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsAndCookieBlockingTrackingProtection() throws IOException {
        enableTrackingProtection();
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions_TrackingProtection");
    }

    /** Tests PageInfo on a website with default setting permissions. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithDefaultSettingPermissions() throws IOException {
        addDefaultSettingPermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_DefaultSettingPermissions");
    }

    /** Tests PageInfo on a website with previous history entries. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithHistory() throws IOException {
        addSomeHistoryEntries();
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_History");
    }

    /** Tests the connection info page of the PageInfo UI - insecure website. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Icon rendering blurry at times: crbug.com/1491905")
    public void testShowConnectionInfoSubpageInsecure() throws IOException {
        mTestServerRule.setServerUsesHttps(false);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_connection_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("The identity of this website isn't verified.")),
                        isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ConnectionInfoSubpageInsecure");
    }

    /** Tests the connection info page of the PageInfo UI - secure website. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowConnectionInfoSubpageSecure() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_connection_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Test Root CA issued this website's certificate.")),
                        isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ConnectionInfoSubpageSecure");
    }

    /** Tests the connection info page of the PageInfo UI - expired certificate. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowConnectionInfoSubpageExpiredCert() throws IOException {
        mTestServerRule.setCertificateType(ServerCertificate.CERT_EXPIRED);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_connection_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Server's certificate has expired.")),
                        isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ConnectionInfoSubpageExpiredCert");
    }

    /** Tests the permissions page of the PageInfo UI with permissions. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowPermissionsSubpage() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsSubpage");
    }

    /** Tests the permissions page of the PageInfo UI with sound permissions. */
    @Test
    @MediumTest
    public void testShowPermissionsSubpageWithSound() throws IOException {
        GURL url = new GURL(mTestServerRule.getServer().getURL("/"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.SOUND,
                            url,
                            url,
                            ContentSettingValues.BLOCK);
                });
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        onView(allOf(withText(containsString("Sound")), isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ContentFeatureList.ONE_TIME_PERMISSION)
    public void testShowPermissionsSubpageWithEphemeralGrantAndPersistentGrant()
            throws IOException {
        GURL url = new GURL(mTestServerRule.getServer().getURL("/"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .setEphemeralGrantForTesting(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    ContentSettingsType.GEOLOCATION,
                                    url,
                                    url);
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.MEDIASTREAM_CAMERA,
                            url,
                            url,
                            ContentSettingValues.ALLOW);
                });
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        onView(withText("Location")).check(matches(hasSibling(withText("Allowed this time"))));
        onView(withText("Camera")).check(matches(hasSibling(withText("Allowed"))));
    }

    /** Tests the cookies page of the PageInfo UI with the Cookie Controls UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1510968")
    public void testShowCookiesSubpageUserBypassOn() throws IOException {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Cookies and other site data are used")),
                        isDisplayed()));
        // Verify that the pref was recorded successfully.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getBoolean(IN_CONTEXT_COOKIE_CONTROLS_OPENED));
                });
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_Toggle_On");
    }

    /** Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageTrackingProtection() throws IOException {
        enableTrackingProtection();
        setBlockAll3PC(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Chrome limits most sites from using")),
                        isDisplayed()));
        // Verify that the pref was recorded successfully.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getBoolean(IN_CONTEXT_COOKIE_CONTROLS_OPENED));
                });
        mRenderTestRule.render(getPageInfoView(), "PageInfo_TrackingProtectionSubpage_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_TrackingProtectionSubpage_Toggle_On");
    }

    private void launchAndCheckTrackingProtectionLaunchUI() {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Chrome limits most sites from using")),
                        isDisplayed()));
        // Verify that the pref was recorded successfully.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getBoolean(IN_CONTEXT_COOKIE_CONTROLS_OPENED));
                });
    }

    /** Same as the previous one but with IP Protection feature enabled. */
    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.IP_PROTECTION_USER_BYPASS,
        ChromeFeatureList.IP_PROTECTION_V1
    })
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/330745124: only 3PC status is implemented in the TPF UI")
    public void testShowCookiesSubpageTrackingProtectionLaunchIPP() throws IOException {
        setBlockAll3PC(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.IP_PROTECTION_ENABLED, true);
                });
        launchAndCheckTrackingProtectionLaunchUI();
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionLaunchIPP_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("You have extra protections")), isDisplayed()));
        onView(withText(containsString("You have extra protections"))).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_TrackingProtectionLaunchIPP_Toggle_On");
    }

    /** Same as the previous one but with Fingerprinting Protection feature enabled. */
    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.FINGERPRINTING_PROTECTION_USER_BYPASS,
    })
    @Features.DisableFeatures(ChromeFeatureList.IP_PROTECTION_V1)
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/330745124: only 3PC status is implemented in the TPF UI")
    public void testShowCookiesSubpageTrackingProtectionLaunchFPP() throws IOException {
        setBlockAll3PC(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED, true);
                });
        launchAndCheckTrackingProtectionLaunchUI();
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionLaunchFPP_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("You have extra protections")), isDisplayed()));
        onView(withText(containsString("You have extra protections"))).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_TrackingProtectionLaunchFPP_Toggle_On");
    }

    /** Same as the previous one but with both IP and Fingerprinting Protection features enabled. */
    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.IP_PROTECTION_USER_BYPASS,
        ChromeFeatureList.IP_PROTECTION_V1,
        ChromeFeatureList.FINGERPRINTING_PROTECTION_USER_BYPASS,
    })
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/330745124: only 3PC status is implemented in the TPF UI")
    public void testShowCookiesSubpageTrackingProtectionLaunchFPPIPP() throws IOException {
        setBlockAll3PC(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.IP_PROTECTION_ENABLED, true);
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED, true);
                });
        launchAndCheckTrackingProtectionLaunchUI();
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionLaunchFPPIPP_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("You have extra protections")), isDisplayed()));
        onView(withText(containsString("You have extra protections"))).perform(click());
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionLaunchFPPIPP_Toggle_On");
    }

    /** Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageTrackingProtectionBlockAll() throws IOException {
        enableTrackingProtection();
        setBlockAll3PC(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(withText(containsString("You blocked sites from using")), isDisplayed()));
        // Verify that the pref was recorded successfully.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getBoolean(IN_CONTEXT_COOKIE_CONTROLS_OPENED));
                });
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionSubpage_Block_All_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_TrackingProtectionSubpage_Block_All_Toggle_On");
    }

    /** Tests the history page of the PageInfo UI. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowHistorySubpage() throws IOException {
        addSomeHistoryEntries();
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("www.example.com", "/"));
        onViewWaiting(allOf(withText(containsString("Last visited")), isDisplayed()));
        onView(withId(PageInfoHistoryController.HISTORY_ROW_ID)).perform(click());
        onViewWaiting(allOf(withText(containsString("Jun 4, 2021")), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_HistorySubpage");
    }

    /**
     * Tests that the permissions page of the PageInfo UI is gone when there are no permissions set.
     */
    @Test
    @MediumTest
    public void testNoPermissionsSubpage() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    /** Tests clearing cookies on the cookies page of the PageInfo UI. */
    @Test
    @MediumTest
    public void testClearCookiesOnSubpage() throws Exception {
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        // Create cookies.
        expectHasCookies(false);
        createCookies();
        expectHasCookies(true);
        // Go to cookies subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that cookies usage is displayed.
        onViewWaiting(allOf(withText(containsString("stored data")), isDisplayed()));
        // Clear cookies in page info.
        onView(withText(containsString("stored data"))).perform(click());
        onView(withText("Delete")).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    /** Tests clearing cookies on the cookies page of the PageInfo UI with User Bypass enabled. */
    @Test
    @MediumTest
    public void testClearCookiesOnSubpageUserBypass() throws Exception {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        // Create cookies.
        expectHasCookies(false);
        createCookies();
        expectHasCookies(true);
        // Go to cookies subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that cookies usage is displayed.
        onViewWaiting(allOf(withText(containsString("stored data")), isDisplayed()));
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        // Clear cookies in page info.
        onView(withText(containsString("stored data"))).perform(click());
        onViewWaiting(allOf(withText("Delete"), isDisplayed()));
        onView(withText("Delete")).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    /** Tests clearing cookies on the Tracking Protection page of the PageInfo UI. */
    @Test
    @MediumTest
    public void testClearCookiesOnSubpageTrackingProtection() throws Exception {
        enableTrackingProtection();
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        // Create cookies.
        expectHasCookies(false);
        createCookies();
        expectHasCookies(true);
        // Go to cookies subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that cookies usage is displayed.
        onViewWaiting(allOf(withText(containsString("stored data")), isDisplayed()));
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        // Clear cookies in page info.
        onView(withText(containsString("stored data"))).perform(click());
        onViewWaiting(allOf(withText("Delete"), isDisplayed()));
        onView(withText("Delete")).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    /** Tests resetting permissions on the permissions page of the PageInfo UI. */
    @Test
    @MediumTest
    public void testResetPermissionsOnSubpage() throws Exception {
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        String url = mTestServerRule.getServer().getURL("/");
        // Create permissions.
        expectHasPermissions(url, false);
        addSomePermissions(url);
        expectHasPermissions(url, true);
        // Go to permissions subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_permissions_row)).inRoot(isDialog()).perform(click());
        // Clear permissions in page info.
        onViewWaiting(allOf(withText("Reset permissions"), isDisplayed())).perform(click());
        onView(withText("Reset")).perform(click());
        // Wait until the UI navigates back and check permissions are reset.
        onViewWaiting(allOf(withId(R.id.page_info_row_wrapper), isDisplayed()));
        // Make sure that the permission section is gone because there are no longer
        // exceptions.
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(withEffectiveVisibility(GONE)));
        expectHasPermissions(url, false);
    }

    /**
     * Test that enabling the federated identity permission in the PageInfo UI clears the embargo.
     */
    @Test
    @MediumTest
    public void testClearFederatedIdentityEmbargoOnSubpage() throws Exception {
        String rpUrl = mTestServerRule.getServer().getURL(sSimpleHtml);
        sActivityTestRule.loadUrl(rpUrl);

        assertTrue(
                getNonWildcardContentSettingExceptions(ContentSettingsType.FEDERATED_IDENTITY_API)
                        .isEmpty());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FederatedIdentityTestUtils.embargoFedCmForRelyingParty(new GURL(rpUrl));
                });
        {
            List<ContentSettingException> exceptions =
                    getNonWildcardContentSettingExceptions(
                            ContentSettingsType.FEDERATED_IDENTITY_API);
            assertEquals(1, exceptions.size());
            assertEquals(GURLUtils.getOrigin(rpUrl), exceptions.get(0).getPrimaryPattern() + "/");
            assertEquals(
                    ContentSettingValues.BLOCK, exceptions.get(0).getContentSetting().intValue());
        }

        // Toggle the federated identity permission.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onView(withId(R.id.switchWidget)).perform(click());

        {
            List<ContentSettingException> exceptions =
                    getNonWildcardContentSettingExceptions(
                            ContentSettingsType.FEDERATED_IDENTITY_API);
            assertEquals(1, exceptions.size());
            assertEquals(GURLUtils.getOrigin(rpUrl), exceptions.get(0).getPrimaryPattern() + "/");
            assertEquals(
                    ContentSettingValues.ALLOW, exceptions.get(0).getContentSetting().intValue());
        }
    }

    /** Tests that page info view is shown correctly for paint preview pages. */
    @Test
    @MediumTest
    public void testPaintPreview() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeActivity activity = sActivityTestRule.getActivity();
                    final Tab tab = activity.getActivityTab();
                    ChromePageInfoControllerDelegate pageInfoControllerDelegate =
                            new ChromePageInfoControllerDelegate(
                                    activity,
                                    tab.getWebContents(),
                                    activity::getModalDialogManager,
                                    new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                                    null,
                                    null,
                                    ChromePageInfoHighlight.noHighlight(),
                                    null) {
                                @Override
                                public boolean isShowingPaintPreviewPage() {
                                    return true;
                                }
                            };
                    PageInfoController.show(
                            sActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(
                allOf(withText(R.string.page_info_connection_paint_preview), isDisplayed()), true);
    }

    /** Tests that page info view is shown correctly for transient pdf pages. */
    @Test
    @MediumTest
    public void testTransientPdfPage() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeActivity activity = sActivityTestRule.getActivity();
                    final Tab tab = activity.getActivityTab();
                    ChromePageInfoControllerDelegate pageInfoControllerDelegate =
                            new ChromePageInfoControllerDelegate(
                                    activity,
                                    tab.getWebContents(),
                                    activity::getModalDialogManager,
                                    new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                                    null,
                                    null,
                                    ChromePageInfoHighlight.noHighlight(),
                                    null) {
                                @Override
                                public @PdfPageType int getPdfPageType() {
                                    return PdfPageType.TRANSIENT_SECURE;
                                }
                            };
                    PageInfoController.show(
                            sActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(
                allOf(withText(R.string.page_info_connection_transient_pdf), isDisplayed()), true);
    }

    /** Tests that page info view is shown correctly for insecure transient pdf pages. */
    @Test
    @MediumTest
    public void testInsecureTransientPdfPage() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeActivity activity = sActivityTestRule.getActivity();
                    final Tab tab = activity.getActivityTab();
                    ChromePageInfoControllerDelegate pageInfoControllerDelegate =
                            new ChromePageInfoControllerDelegate(
                                    activity,
                                    tab.getWebContents(),
                                    activity::getModalDialogManager,
                                    new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                                    null,
                                    null,
                                    ChromePageInfoHighlight.noHighlight(),
                                    null) {
                                @Override
                                public @PdfPageType int getPdfPageType() {
                                    return PdfPageType.TRANSIENT_INSECURE;
                                }
                            };
                    PageInfoController.show(
                            sActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(
                allOf(
                        withText(R.string.page_info_connection_transient_pdf_insecure),
                        isDisplayed()),
                true);
    }

    /** Tests that page info view is shown correctly for local pdf pages. */
    @Test
    @MediumTest
    public void testLocalPdfPage() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeActivity activity = sActivityTestRule.getActivity();
                    final Tab tab = activity.getActivityTab();
                    ChromePageInfoControllerDelegate pageInfoControllerDelegate =
                            new ChromePageInfoControllerDelegate(
                                    activity,
                                    tab.getWebContents(),
                                    activity::getModalDialogManager,
                                    new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                                    null,
                                    null,
                                    ChromePageInfoHighlight.noHighlight(),
                                    null) {
                                @Override
                                public @PdfPageType int getPdfPageType() {
                                    return PdfPageType.LOCAL;
                                }
                            };
                    PageInfoController.show(
                            sActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(
                allOf(withText(R.string.page_info_connection_local_pdf), isDisplayed()), true);
    }

    /** Tests PageInfo on a website with permissions and no particular row highlight. */
    @Test
    @MediumTest
    public void testShowWithPermissionsAndWithoutHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(
                mTestServerRule.getServer().getURL(sSimpleHtml),
                PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_permissions_row))
                .inRoot(isDialog())
                .check(matches(not(hasBackgroundColor(R.color.iph_highlight_blue))));
    }

    /**
     * Tests PageInfo on a website with permissions and a particular permission row highlight.
     * Geolocation is blocked system wide in this test.
     */
    @Test
    @MediumTest
    public void testShowWithPermissionsAndHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(
                mTestServerRule.getServer().getURL(sSimpleHtml), ContentSettingsType.GEOLOCATION);
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(hasBackgroundColor(R.color.iph_highlight_blue)));
    }

    /**
     * Tests the permissions page of the PageInfo UI with permissions and a particular permission
     * row highlight.
     */
    @Test
    @MediumTest
    public void testShowPermissionsSubpageWithHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(
                mTestServerRule.getServer().getURL(sSimpleHtml), ContentSettingsType.GEOLOCATION);
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        Context context = ApplicationProvider.getApplicationContext();
        // Find the preference and check its background color.
        onView(
                        allOf(
                                withParent(withId(R.id.recycler_view)),
                                hasDescendant(
                                        withText(
                                                context.getString(
                                                        R.string
                                                                .website_settings_device_location)))))
                .check(matches(hasBackgroundColor(R.color.iph_highlight_blue)));
    }

    /**
     * Tests that a close button is shown with accessibility enabled and that it closes the dialog.
     */
    @Test
    @MediumTest
    public void testCloseButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
                });
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertTrue(controller.isDialogShowingForTesting());
        onView(withId(R.id.page_info_close)).perform(click());
        assertFalse(controller.isDialogShowingForTesting());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null);
                });
    }

    /** Tests the summary string of the history page of the PageInfo UI. */
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(HistorySummaryTestParams.class)
    public void testHistorySummaryText(long timestamp, String expectedSummary) throws IOException {
        StubbedHistoryProvider historyProvider = new StubbedHistoryProvider();
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, timestamp));
        PageInfoHistoryController.setProviderForTests(historyProvider);
        PageInfoHistoryController.setClockForTesting(
                () -> {
                    return sTimestampJune4;
                });

        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("www.example.com", "/"));
        onViewWaiting(allOf(withText(containsString(expectedSummary)), isDisplayed()));
    }

    /** Tests clicking on a history item from the history page of the PageInfo UI. */
    @Test
    @MediumTest
    public void testHistorySubpageItemClick() throws Exception {
        StubbedHistoryProvider historyProvider = new StubbedHistoryProvider();
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, sTimestampJune4));
        HistoryContentManager.setProviderForTests(historyProvider);
        PageInfoHistoryController.setProviderForTests(historyProvider);
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("www.example.com", "/"));

        final CallbackHelper onDidStartNavigationHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new WebContentsObserver(sActivityTestRule.getWebContents()) {
                        @Override
                        public void didStartNavigationInPrimaryMainFrame(
                                NavigationHandle navigationHandle) {
                            if (navigationHandle.getUrl().getHost().equals("www.example.com")) {
                                onDidStartNavigationHelper.notifyCalled();
                            }
                        }
                    };
                });
        onViewWaiting(allOf(withText(containsString("Last visited")), isDisplayed()));
        onView(withId(PageInfoHistoryController.HISTORY_ROW_ID)).perform(click());
        onViewWaiting(allOf(withText(containsString("Jun 4, 2021")), isDisplayed()));
        int callCount = onDidStartNavigationHelper.getCallCount();
        onView(withText("www.example.com")).perform(click());
        onDidStartNavigationHelper.waitForCallback(callCount);
    }

    /** Tests PageInfo on a website with ad personalization info. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowAdPersonalizationInfo() throws IOException {
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("example.com", sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_AdPersonalization");
    }

    /** Tests ad personalization subpage. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowAdPersonalizationInfoSubPageV4() throws IOException {
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("example.com", sSimpleHtml));
        onView(withId(PageInfoAdPersonalizationController.ROW_ID))
                .inRoot(isDialog())
                .perform(click());
        onViewWaiting(
                allOf(
                        withText(R.string.page_info_ad_privacy_subpage_manage_button),
                        isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_AdPersonalizationSubPageV4");
    }

    /** Tests opening ad personalization settings. */
    @Test
    @MediumTest
    public void testOpenAdPersonalizationSettingsV4() throws IOException {
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("example.com", sSimpleHtml));
        onView(withId(PageInfoAdPersonalizationController.ROW_ID)).perform(click());
        onViewWaiting(
                        allOf(
                                withText(R.string.page_info_ad_privacy_subpage_manage_button),
                                isDisplayed()))
                .perform(click());
        // Check that settings are displayed.
        onView(withText(R.string.ad_privacy_page_topics_link_row_label))
                .check(matches(isDisplayed()));
        // Leave settings view.
        onView(withContentDescription("Navigate up")).perform(click());
        onView(withText(R.string.ad_privacy_page_topics_link_row_label)).check(doesNotExist());
    }

    // TODO(crbug.com/40685274): Add tests for preview pages, offline pages, offline
    // state and other
    // states.
}
