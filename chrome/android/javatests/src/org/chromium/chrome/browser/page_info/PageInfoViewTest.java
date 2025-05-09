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
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;

import static org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings.EXTRA_SITE;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.content_settings.PrefNames.IN_CONTEXT_COOKIE_CONTROLS_OPENED;
import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;
import static org.chromium.ui.test.util.ViewUtils.hasBackgroundColor;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.view.Gravity;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Root;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.FederatedIdentityTestUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.history.HistoryContentManager;
import org.chromium.chrome.browser.history.StubbedHistoryProvider;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfPageType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.RwsCookieInfo;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoAdPersonalizationController;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoCookiesController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.GURLUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
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
// TODO(crbug.com/344672095): Failing when batched, batch this again.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
@EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_UI})
// Disable TrackingProtection3pcd as we use prefs instead of the feature in
// these tests.
@DisableFeatures({ChromeFeatureList.TRACKING_PROTECTION_3PCD})
public class PageInfoViewTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";
    private static final String sSiteDataHtml = "/content/test/data/browsing_data/site_data.html";

    private static final int DAYS_UNTIL_EXPIRATION = 33;

    private static final String[] sCookieDataTypes = {
        "Cookie", "LocalStorage", "ServiceWorker", "CacheStorage", "IndexedDb", "FileSystem"
    };

    // June 4, 2021 12:00:00 GMT+00:00
    private static final long TIMESTAMP_JUNE_4 = 1622808000000L;
    // April 4, 2021 12:00:00 GMT+00:00
    private static final long TIMESTAMPE_APRIL_4 = 1617537600000L;

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
            timestamp = CalendarUtils.getStartOfDay(TIMESTAMP_JUNE_4).getTime().getTime();
            parameters.add(
                    new ParameterSet()
                            .value(
                                    timestamp,
                                    res.getString(R.string.page_info_history_last_visit_today))
                            .name("Today"));
            timestamp = TIMESTAMP_JUNE_4 - 1 * DateUtils.DAY_IN_MILLIS;
            parameters.add(
                    new ParameterSet()
                            .value(
                                    timestamp,
                                    res.getString(R.string.page_info_history_last_visit_yesterday))
                            .name("Yesterday"));
            int offset = random.nextInt(6) + 2;
            timestamp = TIMESTAMP_JUNE_4 - offset * DateUtils.DAY_IN_MILLIS;
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
                                    TIMESTAMPE_APRIL_4,
                                    res.getString(
                                            R.string.page_info_history_last_visit_date,
                                            StringUtils.dateToHeaderString(
                                                    new Date(TIMESTAMPE_APRIL_4))))
                            .name("ExactDay"));
            return parameters;
        }
    }

    @Mock private SettingsNavigation mSettingsNavigation;

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

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
        mActivityTestRule.loadUrl(url);
        openPageInfo(highlightedPermission);
    }

    private void openPageInfo(@ContentSettingsType.EnumType int highlightedPermission) {
        ChromeActivity activity = mActivityTestRule.getActivity();
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
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        View view = controller.getPageInfoView();
        assertNotNull(view);
        return view;
    }

    private void enableTrackingProtectionFixedExpiration(
            boolean isModeBUiInCookiesController, int days) {
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        var tpController = controller.getCookiesController();
        tpController.setFixedExceptionExpirationForTesting(true);
        tpController.setDaysUntilExpirationForTesting(days);
    }

    private void enableModeBUiInCookiesController() {
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        var tpController = controller.getCookiesController();
        tpController.setIsModeBUiForTesting(true);
    }

    private void enableIsIncognitoInCookiesController() {
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        var tpController = controller.getCookiesController();
        tpController.setIsIncognitoForTesting(true);
    }

    private void enableTpcdGrantEnforcement() {
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        var tpController = controller.getCookiesController();
        tpController.setEnforcementForTesting(CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT);
    }

    private RwsCookieInfo getRwsCookieInfo(String url) {
        Website rwsOwnerWebsite =
                new Website(
                        WebsiteAddress.create(
                                mTestServerRule.getServer().getURLWithHostName(url, "/")),
                        null);
        Website rwsMemberWebsite =
                new Website(
                        WebsiteAddress.create(
                                mTestServerRule
                                        .getServer()
                                        .getURLWithHostName(("prefix." + url), "/")),
                        null);
        RwsCookieInfo rwsInfo =
                new RwsCookieInfo(
                        rwsOwnerWebsite.getAddress().getDomainAndRegistry(),
                        List.of(rwsOwnerWebsite, rwsMemberWebsite));
        rwsOwnerWebsite.setRwsCookieInfo(rwsInfo);
        rwsMemberWebsite.setRwsCookieInfo(rwsInfo);
        return rwsInfo;
    }

    private PageInfoCookiesController getCookiesController() {
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertNotNull(controller);
        return controller.getCookiesController();
    }

    private void setRwsInfo(String url) {
        getCookiesController().setRwsInfoForTesting(getRwsCookieInfo(url).getMembers());
    }

    private void setRwsInfoWithWebsite(Website site) {
        RwsCookieInfo rwsInfo =
                new RwsCookieInfo(site.getAddress().getDomainAndRegistry(), List.of(site));
        site.setRwsCookieInfo(rwsInfo);
        getCookiesController().setRwsInfoForTesting(rwsInfo.getMembers());
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

    private void setBlockAll3pc(boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED, value);
                });
    }

    private String runJavascriptAsync(String type) throws TimeoutException {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                mActivityTestRule.getWebContents(), type);
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
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, TIMESTAMPE_APRIL_4));
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, TIMESTAMP_JUNE_4));
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

        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        PrivacySandboxBridgeJni.setInstanceForTesting(mFakePrivacySandboxBridge);

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

    /** Tests PageInfo on an allowlisted website */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowTrackingProtectionStatusSubtitleOnAllowlistedSiteModeB()
            throws IOException {
        enableTrackingProtection();
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTpcdGrantEnforcement();
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_CookiesSubpageSubtitle_AllowlistedSite");
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

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void showRwsButtonWhenRwsEnabled() throws IOException {
        String hostName = "example.com";
        String url = mTestServerRule.getServer().getURLWithHostName(hostName, "/");
        loadUrlAndOpenPageInfo(url);
        setRwsInfo(hostName);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(withText(R.string.page_info_rws_v2_button_title), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_RwsEnabled");
    }

    @Test
    @MediumTest
    public void shouldNavigateToSiteSettingsWhenRwsButtonClicked() throws IOException {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        String hostName = "example.com";
        String url = mTestServerRule.getServer().getURLWithHostName(hostName, "/");
        Website currentSite = new Website(WebsiteAddress.create(url), null);
        loadUrlAndOpenPageInfo(url);
        setRwsInfoWithWebsite(currentSite);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(withText(R.string.page_info_rws_v2_button_title), isDisplayed()));
        Context context = ApplicationProvider.getApplicationContext();
        String subtitle =
                context.getString(R.string.page_info_rws_v2_button_subtitle_android, hostName);
        onViewWaiting(allOf(withText(subtitle), isDisplayed()));
        onView(withText(R.string.page_info_rws_v2_button_title)).perform(click());
        Bundle extras = new Bundle();
        extras.putSerializable(EXTRA_SITE, currentSite);
        Mockito.verify(mSettingsNavigation)
                .startSettings(any(), eq(SingleWebsiteSettings.class), refEq(extras));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void showRwsButtonWhenRwsEnabledAndCurrentSetManaged() throws IOException {
        mFakePrivacySandboxBridge.setIsRwsManaged(true);
        String hostName = "example.com";
        String url = mTestServerRule.getServer().getURLWithHostName(hostName, "/");
        loadUrlAndOpenPageInfo(url);
        setRwsInfo(hostName);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(withText(R.string.page_info_rws_v2_button_title), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_RwsEnabledAndManaged");
    }

    @Test
    @MediumTest
    public void shouldNavigateToSiteSettingsWhenRwsManagedAndButtonClicked() throws IOException {
        mFakePrivacySandboxBridge.setIsRwsManaged(true);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        UserActionTester userActionTester = new UserActionTester();
        String hostName = "example.com";
        String url = mTestServerRule.getServer().getURLWithHostName(hostName, "/");
        Website currentSite = new Website(WebsiteAddress.create(url), null);
        loadUrlAndOpenPageInfo(url);
        setRwsInfoWithWebsite(currentSite);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(withText(R.string.page_info_rws_v2_button_title), isDisplayed()));
        Context context = ApplicationProvider.getApplicationContext();
        String subtitle =
                context.getString(R.string.page_info_rws_v2_button_subtitle_android, hostName);
        onViewWaiting(allOf(withText(subtitle), isDisplayed()));
        onView(withText(R.string.page_info_rws_v2_button_title)).perform(click());
        Bundle extras = new Bundle();
        extras.putSerializable(EXTRA_SITE, currentSite);
        Mockito.verify(mSettingsNavigation)
                .startSettings(any(), eq(SingleWebsiteSettings.class), refEq(extras));
        assertEquals(
                1,
                userActionTester.getActionCount("PageInfo.CookiesSubpage.RwsSiteSettingsOpened"));
        userActionTester.tearDown();
    }

    /** Tests the cookies page of the PageInfo UI with the Cookie Controls UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
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
        setBlockAll3pc(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(false, 33);
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

    /**
     * Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled and 3pcs
     * not blocked.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCookiesSubpageTitleDescriptionWhenModeBEnabledInCookiesController()
            throws IOException {
        setBlockAll3pc(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Chrome limits most sites from using")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpageTitleDescription_ModeBEnabled_3pcLimited");
    }

    /**
     * Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled and 3pcs
     * are blocked.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCookiesSubpageTitleDescriptionWhenModeBEnabledInCookiesControllerAnd3pcBlocked()
            throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString("You blocked sites from using third-party cookies")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpageTitleDescription_ModeBEnabled_3pcBlocked");
    }

    /**
     * Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled and is in
     * incognito.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void
            testCookiesSubpageTitleDescriptionWhenModeBEnabledInCookiesControllerAndIsIncognito()
                    throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableIsIncognitoInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString(
                                        "Chrome blocks sites from using third-party cookies")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpageTitleDescription_ModeBEnabled_Incognito");
    }

    /** Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCookiesSubpageTitleDescriptionWhenModeDisabledInCookiesController()
            throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString(
                                        "Cookies and other site data are used to remember you")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(), "PageInfo_CookiesSubpageTitleDescription_ModeBDisabled");
    }

    /**
     * Tests the cookies top level page subtitle of the PageInfo UI with the Tracking Protection UI
     * enabled and cookies blocked.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageSubtitleLimitedInModeB() throws IOException {
        enableTrackingProtection();
        setBlockAll3pc(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(false, 33);
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        onView(withText(containsString("Third-party cookies limited")))
                .check(matches(isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_Subtitle_Limited_ModeB");
    }

    /**
     * Tests the cookies top level page subtitle of the PageInfo UI with the Tracking Protection UI
     * enabled and cookies blocked.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageSubtitleBlockedInModeBCookiesController() throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        onView(withText(containsString("Third-party cookies blocked")))
                .check(matches(isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage_Subtitle_Blocked_ModeB");
    }

    /**
     * Tests the cookies page description of the PageInfo UI with the Tracking Protection UI
     * enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageWillLimitDescriptionInModeB() throws IOException {
        setBlockAll3pc(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(true, 33);
        enableModeBUiInCookiesController();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onView(withText(containsString("Limited"))).check(matches(isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageLimitedDescription_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("days until Chrome limits cookies again")),
                        isDisplayed()));
        onView(withText(containsString("Allowed"))).check(matches(isDisplayed()));

        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageLimitedDescription_Toggle_On");
    }

    /**
     * Tests the cookies page description of the PageInfo UI with the Tracking Protection UI
     * enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageWillLimitTomorrowDescriptionInModeB() throws IOException {
        setBlockAll3pc(false);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(true, 1);
        enableModeBUiInCookiesController();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Chrome will limit cookies again tomorrow")),
                        isDisplayed()));
        onView(withText(containsString("Allowed"))).check(matches(isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageLimitedTomorrowDescription_Toggle_On");
    }

    /**
     * Tests the cookies top level page subtitle description of the PageInfo UI with the Tracking
     * Protection UI disabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubtitleDescriptionModeBDisabled() throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(true, 33);
        // Check that the cookie toggle is displayed and try clicking it.
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString(
                                        "Help us improve Chrome by telling us why you allowed")),
                        isDisplayed()));
        onView(withText(containsString("Allowed"))).check(matches(isDisplayed()));

        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpage_SubtitleDescription_ModeBDisabled_ToggleOn");
    }

    /**
     * Tests the cookies top level page subtitle description of the PageInfo UI with the Tracking
     * Protection UI enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubtitleDescriptionModeBEnabled() throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());

        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString(
                                        "You temporarily allowed this site to use third-party")),
                        isDisplayed()));
        onView(withText(containsString("Allowed"))).check(matches(isDisplayed()));

        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpage_SubtitleDescription_ModeBEnabled_ToggleOn");
    }

    /**
     * Tests the cookies page description of the PageInfo UI with the Tracking Protection UI enabled
     * and cookies blocked in PageInfoCookiesController.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageWillBlockDescriptionInModeBCookiesController()
            throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableModeBUiInCookiesController();
        enableTrackingProtectionFixedExpiration(true, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onView(withText(containsString("Blocked"))).check(matches(isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageBlockedDescriptionCookiesController_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("days until cookies are blocked again")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageBlockedDescriptionCookiesController_Toggle_On");
    }

    /**
     * Tests the cookies page description of the PageInfo UI with the Tracking Protection UI enabled
     * and cookies blocked for 1 day in PageInfoCookiesController.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageWillBlockTomorrowDescriptionInModeBCookiesController()
            throws IOException {
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(true, 1);
        enableModeBUiInCookiesController();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("Chrome will block cookies again tomorrow")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageBlockedTomorrowDescriptionCookiesController_Toggle_On");
    }

    /**
     * Tests the cookies page description of the PageInfo UI with the Tracking Protection UI enabled
     * and cookies blocked.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageWillBlockDescriptionInModeB() throws IOException {
        enableTrackingProtection();
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(false, 33);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageBlockedDescription_Toggle_Off");
        // Check that the cookie toggle is displayed and try clicking it.
        onViewWaiting(allOf(withText(containsString("Third-party cookies")), isDisplayed()));
        onView(withText(containsString("Third-party cookies"))).perform(click());
        onViewWaiting(
                allOf(
                        withText(containsString("days until cookies are blocked again")),
                        isDisplayed()));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_TrackingProtectionSubpageBlockedDescription_Toggle_On");
    }

    /** Tests PageInfo on an allowlisted website */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void cookiesSubpageShowsGrantDescriptionForAllowlistedSiteInModeB() throws IOException {
        enableTrackingProtection();
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTpcdGrantEnforcement();
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(
                allOf(
                        withText(
                                containsString(
                                        "Chrome limits most sites from using third-party cookies")),
                        isDisplayed()));
        onView(withText(containsString("manage access to third-party cookies")))
                .perform(clickOnClickableSpan(0));
        mRenderTestRule.render(
                getPageInfoView(),
                "PageInfo_CookiesSubpageTrackingProtectionGrantDescription_AllowlistedSite");
    }

    /** Tests the cookies page of the PageInfo UI with the Tracking Protection UI enabled. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowCookiesSubpageTrackingProtectionBlockAll() throws IOException {
        enableTrackingProtection();
        setBlockAll3pc(true);
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        enableTrackingProtectionFixedExpiration(false, 33);
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
    public void clearCookiesOnSubpage() throws Exception {
        mActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
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
    public void clearCookiesOnSubpageUserBypass() throws Exception {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        mActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
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
        onViewWaiting(
                allOf(
                        withText(R.string.page_info_cookies_clear_confirmation_button),
                        isDisplayed()));
        onView(withText(R.string.page_info_cookies_clear_confirmation_button)).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    /** Tests clearing cookies on the Tracking Protection page of the PageInfo UI. */
    @Test
    @MediumTest
    public void clearCookiesOnSubpageTrackingProtection() throws Exception {
        enableTrackingProtection();
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        mActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
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
        onViewWaiting(
                allOf(
                        withText(R.string.page_info_cookies_clear_confirmation_button),
                        isDisplayed()));
        onView(withText(R.string.page_info_cookies_clear_confirmation_button)).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    static Matcher<View> hasAccessibilityLiveRegion(int liveRegionState) {
        return new TypeSafeMatcher<>() {
            @Override
            protected boolean matchesSafely(View view) {
                return view.getAccessibilityLiveRegion() == liveRegionState;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("View has live region state " + liveRegionState);
            }
        };
    }

    /** Tests that the User Bypass has an accessibility live region set up correctly. */
    @Test
    @MediumTest
    public void a11yLiveRegionInUserBypass() throws Exception {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        mActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        // Create cookies.
        expectHasCookies(false);
        createCookies();
        expectHasCookies(true);
        // Go to cookies subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        enableTrackingProtectionFixedExpiration(
                /* isModeBUiInCookiesController= */ false, /* days= */ DAYS_UNTIL_EXPIRATION);
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        // Check that cookies usage is displayed.
        onViewWaiting(allOf(withText(containsString("stored data")), isDisplayed()));
        // Check that the cookie toggle is displayed.
        onViewWaiting(
                allOf(
                        withText(R.string.page_info_tracking_protection_toggle_blocked),
                        isDisplayed()));
        // Verify the a11y live region.
        onView(
                        withText(
                                R.string
                                        .page_info_cookies_site_not_working_description_tracking_protection))
                .check(matches(hasAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE)));
        // Click on the toggle for the content to change.
        onView(withText(R.string.page_info_tracking_protection_toggle_blocked)).perform(click());
        // Verify the a11y live region.
        Context context = ApplicationProvider.getApplicationContext();
        String description =
                context.getString(R.string.page_info_cookies_send_feedback_description)
                        .replaceAll("<link>|</link>", "");
        onView(withText(description))
                .check(matches(hasAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE)));
    }

    /** Tests resetting permissions on the permissions page of the PageInfo UI. */
    @Test
    @MediumTest
    public void testResetPermissionsOnSubpage() throws Exception {
        mActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
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
        mActivityTestRule.loadUrl(rpUrl);

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
            assertEquals(ContentSettingValues.BLOCK, exceptions.get(0).getContentSetting());
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
            assertEquals(ContentSettingValues.ALLOW, exceptions.get(0).getContentSetting());
        }
    }

    /** Tests that page info view is shown correctly for paint preview pages. */
    @Test
    @MediumTest
    public void testPaintPreview() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeActivity activity = mActivityTestRule.getActivity();
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
                            mActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight(),
                            Gravity.TOP);
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
                    final ChromeActivity activity = mActivityTestRule.getActivity();
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
                            mActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight(),
                            Gravity.TOP);
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
                    final ChromeActivity activity = mActivityTestRule.getActivity();
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
                            mActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight(),
                            Gravity.TOP);
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
                    final ChromeActivity activity = mActivityTestRule.getActivity();
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
                            mActivityTestRule.getActivity(),
                            tab.getWebContents(),
                            null,
                            PageInfoController.OpenedFromSource.MENU,
                            pageInfoControllerDelegate,
                            ChromePageInfoHighlight.noHighlight(),
                            Gravity.TOP);
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
        PageInfoController controller = PageInfoController.getLastPageInfoController();
        assertTrue(controller.isDialogShowing());
        onView(withId(R.id.page_info_close)).perform(click());
        assertFalse(controller.isDialogShowing());
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
                    return TIMESTAMP_JUNE_4;
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
        historyProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, TIMESTAMP_JUNE_4));
        HistoryContentManager.setProviderForTests(historyProvider);
        PageInfoHistoryController.setProviderForTests(historyProvider);
        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("www.example.com", "/"));

        final CallbackHelper onDidStartNavigationHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new WebContentsObserver(mActivityTestRule.getWebContents()) {
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

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testBottomGravity() {
        float cornerRadius =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeActivity activity = mActivityTestRule.getActivity();
                            BrowserControlsManager browserControlsManager =
                                    BrowserControlsManagerSupplier.getValueOrNullFrom(
                                            activity.getWindowAndroid());
                            browserControlsManager.setControlsPosition(
                                    ControlsPosition.BOTTOM,
                                    0,
                                    0,
                                    0,
                                    browserControlsManager.getTopControlsHeight(),
                                    0,
                                    0);
                            return activity.getResources()
                                    .getDimension(R.dimen.page_info_popup_corners_radius);
                        });

        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("example.com", sSimpleHtml));
        Matcher<Root> isBottomMatcher =
                new TypeSafeMatcher<>() {
                    @Override
                    protected boolean matchesSafely(Root root) {
                        return (root.getWindowLayoutParams().get().gravity & Gravity.BOTTOM) != 0;
                    }

                    @Override
                    public void describeTo(Description description) {
                        description.appendText("Root view with bottom gravity");
                    }
                };
        onViewWaiting(instanceOf(FadingEdgeScrollView.class))
                .inRoot(allOf(isDialog(), isBottomMatcher))
                .check(
                        matches(
                                new TypeSafeMatcher<>() {
                                    private final float[] mCornerRadii =
                                            new float[] {
                                                cornerRadius,
                                                cornerRadius,
                                                cornerRadius,
                                                cornerRadius,
                                                0,
                                                0,
                                                0,
                                                0
                                            };

                                    @Override
                                    public void describeTo(Description description) {
                                        description.appendText(
                                                "View with bg drawable with top rounded"
                                                        + " corners");
                                    }

                                    @Override
                                    protected boolean matchesSafely(View view) {
                                        Drawable bg = view.getBackground();
                                        if (!(bg instanceof GradientDrawable drawable)) {
                                            return false;
                                        }

                                        return Arrays.equals(
                                                drawable.getCornerRadii(), mCornerRadii);
                                    }

                                    @Override
                                    public void describeMismatchSafely(
                                            View view, Description description) {
                                        Drawable bg = view.getBackground();
                                        if (!(bg instanceof GradientDrawable drawable)) {
                                            description.appendText("Bg not a GradientDrawable");
                                            return;
                                        }

                                        description.appendText(
                                                "Expected corner radii "
                                                        + Arrays.toString(mCornerRadii)
                                                        + " but received "
                                                        + Arrays.toString(
                                                                drawable.getCornerRadii()));
                                    }
                                }));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.TABLET})
    public void testBottomGravityTablets() {
        ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeActivity activity = mActivityTestRule.getActivity();
                            BrowserControlsManager browserControlsManager =
                                    BrowserControlsManagerSupplier.getValueOrNullFrom(
                                            activity.getWindowAndroid());
                            browserControlsManager.setControlsPosition(
                                    ControlsPosition.BOTTOM,
                                    0,
                                    0,
                                    0,
                                    browserControlsManager.getTopControlsHeight(),
                                    0,
                                    0);
                            return activity.getModalDialogManagerSupplier();
                        });

        loadUrlAndOpenPageInfo(
                mTestServerRule.getServer().getURLWithHostName("example.com", sSimpleHtml));
        assertTrue(modalDialogManagerSupplier.get().isShowing());
        assertEquals(
                PageInfoController.getLastPageInfoController(),
                modalDialogManagerSupplier
                        .get()
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CONTROLLER));
    }

    // TODO(crbug.com/40685274): Add tests for preview pages, offline pages, offline
    // state and other
    // states.
}
