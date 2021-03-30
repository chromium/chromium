// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
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

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.chrome.test.util.ViewUtils.hasBackgroundColor;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.SiteSettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different
 * configurations correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(PER_CLASS)
@Batch.SplitByFeature
public class PageInfoViewTest {
    private static final String TAG = "PageInfoViewTest";

    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";
    private static final String sSiteDataHtml = "/content/test/data/browsing_data/site_data.html";

    private static String[] sCookieDataTypes = {"Cookie", "LocalStorage", "ServiceWorker",
            "CacheStorage", "IndexedDb", "FileSystem", "WebSql"};

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(4).build();

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
            String url, @ContentSettingsType int highlightedPermission) {
        sActivityTestRule.loadUrl(url);
        openPageInfo(highlightedPermission);
    }

    private void openPageInfo(@ContentSettingsType int highlightedPermission) {
        ChromeActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new ChromePageInfo(activity.getModalDialogManagerSupplier(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR)
                    .show(tab, highlightedPermission);
        });

        if (PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_V2)) {
            onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()));
        } else {
            onViewWaiting(allOf(withId(R.id.page_info_url), isDisplayed()));
        }
    }

    private View getPageInfoView() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        View view = controller.getPageInfoViewForTesting();
        assertNotNull(view);
        return view;
    }

    private void setThirdPartyCookieBlocking(@CookieControlsMode int value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setInteger(COOKIE_CONTROLS_MODE, value);
        });
    }

    private String runJavascriptAsync(String type) throws TimeoutException {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                sActivityTestRule.getWebContents(), type);
    }

    private void expectHasCookies(boolean hasData) throws TimeoutException {
        for (String type : sCookieDataTypes) {
            assertEquals(hasData ? "true" : "false", runJavascriptAsync("has" + type + "()"));
        }
    }

    private void createCookies() throws TimeoutException {
        for (String type : sCookieDataTypes) {
            runJavascriptAsync("set" + type + "()");
        }
    }

    private void addSomePermissions(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.GEOLOCATION, url, "*", ContentSettingValues.ALLOW);
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.NOTIFICATIONS, url, "*", ContentSettingValues.BLOCK);
        });
    }

    private void expectHasPermissions(String url, boolean hasPermissions) {
        // The default value for these types is ask.
        @ContentSettingValues
        int expectAllow = hasPermissions ? ContentSettingValues.ALLOW : ContentSettingValues.ASK;
        @ContentSettingValues
        int expectBlock = hasPermissions ? ContentSettingValues.BLOCK : ContentSettingValues.ASK;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals(expectBlock,
                    WebsitePreferenceBridgeJni.get().getSettingForOrigin(
                            Profile.getLastUsedRegularProfile(), ContentSettingsType.NOTIFICATIONS,
                            url, url));
            assertEquals(expectAllow,
                    WebsitePreferenceBridgeJni.get().getSettingForOrigin(
                            Profile.getLastUsedRegularProfile(), ContentSettingsType.GEOLOCATION,
                            url, "*"));
        });
    }

    private void addDefaultSettingPermissions(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.MEDIASTREAM_MIC, url, "*", ContentSettingValues.DEFAULT);
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.MEDIASTREAM_CAMERA, url, "*", ContentSettingValues.ASK);
        });
    }

    private void clearPermissions() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.SITE_SETTINGS}, TimePeriod.ALL_TIME);
        });
        helper.waitForCallback(0);
    }

    @Before
    public void setUp() throws InterruptedException {
        // Some test devices have geolocation disabled. Override LocationUtils for a stable result.
        LocationUtils.setFactory(TestLocationUtils::new);

        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);
    }

    @After
    public void tearDown() throws TimeoutException {
        LocationUtils.setFactory(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null); });
        // Notification channels don't get cleaned up automatically.
        // TODO(crbug.com/951402): Find a general solution to avoid leaking channels between tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SiteChannelsManager manager = SiteChannelsManager.getInstance();
                manager.deleteAllSiteChannels();
            });
        }

        setThirdPartyCookieBlocking(CookieControlsMode.INCOGNITO_ONLY);
        clearPermissions();
    }

    /**
     * Tests PageInfo on an insecure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowOnInsecureHttpWebsite() throws IOException {
        mTestServerRule.setServerUsesHttps(false);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_HttpWebsite");
    }

    /**
     * Tests PageInfo on a secure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowOnSecureWebsite() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsite");
    }

    /**
     * Tests PageInfo on a website with expired certificate.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowOnExpiredCertificateWebsite() throws IOException {
        mTestServerRule.setCertificateType(ServerCertificate.CERT_EXPIRED);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ExpiredCertWebsite");
    }

    /**
     * Tests PageInfo on internal page.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testChromePage() throws IOException {
        loadUrlAndOpenPageInfo("chrome://version/");
        mRenderTestRule.render(getPageInfoView(), "PageInfo_InternalSite");
    }

    /**
     * Tests PageInfo on a website with permissions.
     * Geolocation is blocked system wide in this test.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowWithPermissions() throws IOException {
        mIsSystemLocationSettingEnabled = false;
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions");
    }

    /**
     * Tests PageInfo on a website with cookie controls enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowWithCookieBlocking() throws IOException {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookieBlocking");
    }

    /**
     * Tests PageInfo on a website with cookie controls and permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowWithPermissionsAndCookieBlocking() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsAndCookieBlocking");
    }

    /**
     * Tests PageInfo on a website with default setting permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowWithDefaultSettingPermissions() throws IOException {
        addDefaultSettingPermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_DefaultSettingPermissions");
    }

    /**
     * Tests the new PageInfo UI on a secure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowOnSecureWebsiteV2() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsiteV2");
    }

    /**
     * Tests the connection info page of the new PageInfo UI.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowConnectionInfoSubpage() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_connection_row)).perform(click());
        onViewWaiting(
                allOf(withText(containsString("Test Root CA issued this website's certificate.")),
                        isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ConnectionInfoSubpage");
    }

    /**
     * Tests the permissions page of the new PageInfo UI with permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowPermissionsSubpage() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsSubpage");
    }

    /**
     * Tests the permissions page of the new PageInfo UI with permissions and actionable flag
     * enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(
            {PageInfoFeatureList.PAGE_INFO_V2, SiteSettingsFeatureList.ACTIONABLE_CONTENT_SETTINGS})
    public void
    testShowPermissionsActionableSubpage() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsSubpage_Actionable");
    }

    /**
     * Tests the cookies page of the new PageInfo UI.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testShowCookiesSubpage() throws IOException {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        onViewWaiting(allOf(
                withText(containsString("Cookies and other site data are used")), isDisplayed()));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage");
    }

    /**
     * Tests that the permissions page of the new PageInfo UI is gone when there are no permissions
     * set.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testNoPermissionsSubpage() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    /**
     * Tests clearing cookies on the cookies page of the new PageInfo UI.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
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
        onView(withText("Clear")).perform(click());
        // Wait until the UI navigates back and check cookies are deleted.
        onViewWaiting(allOf(withId(R.id.page_info_cookies_row), isDisplayed()));
        expectHasCookies(false);
    }

    /**
     * Tests resetting permissions on the permissions page of the new PageInfo UI.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testResetPermissionsOnSubpage() throws Exception {
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSiteDataHtml));
        String url = mTestServerRule.getServer().getURL("/");
        // Create permissions.
        expectHasPermissions(url, false);
        addSomePermissions(url);
        expectHasPermissions(url, true);
        // Go to permissions subpage.
        openPageInfo(PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        // Clear permissions in page info.
        onViewWaiting(allOf(withText("Reset permissions"), isDisplayed())).perform(click());
        onView(withText("Reset")).perform(click());
        // Wait until the UI navigates back and check permissions are reset.
        onViewWaiting(allOf(withId(R.id.page_info_row_wrapper), isDisplayed()));
        // Make sure that the permission section is gone because there are no longer exceptions.
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(withEffectiveVisibility(GONE)));
        expectHasPermissions(url, false);
    }

    /**
     * Tests that page info view is shown correctly for paint preview pages.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testPaintPreview() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final ChromeActivity activity = sActivityTestRule.getActivity();
            final Tab tab = activity.getActivityTab();
            ChromePageInfoControllerDelegate pageInfoControllerDelegate =
                    new ChromePageInfoControllerDelegate(activity, tab.getWebContents(),
                            activity::getModalDialogManager,
                            new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)) {
                        @Override
                        public boolean isShowingPaintPreviewPage() {
                            return true;
                        }
                    };
            PageInfoController.show(sActivityTestRule.getActivity(), tab.getWebContents(), null,
                    PageInfoController.OpenedFromSource.MENU, pageInfoControllerDelegate,
                    new ChromePermissionParamsListBuilderDelegate(),
                    PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        });
        onViewWaiting(allOf(withText(R.string.page_info_connection_paint_preview), isDisplayed()));
    }

    /**
     * Tests PageInfo on a website with permissions and no particular row highlight.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(
            {PageInfoFeatureList.PAGE_INFO_V2, PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY})
    public void
    testShowWithPermissionsAndWithoutHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(mTestServerRule.getServer().getURL(sSimpleHtml),
                PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(not(hasBackgroundColor(R.color.iph_highlight_blue))));
    }

    /**
     * Tests PageInfo on a website with permissions and a particular permission row highlight.
     * Geolocation is blocked system wide in this test.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(
            {PageInfoFeatureList.PAGE_INFO_V2, PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY})
    public void
    testShowWithPermissionsAndHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(
                mTestServerRule.getServer().getURL(sSimpleHtml), ContentSettingsType.GEOLOCATION);
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(hasBackgroundColor(R.color.iph_highlight_blue)));
    }

    /**
     * Tests the permissions page of the new PageInfo UI with permissions and a particular
     * permission row highlight.
     */
    @Test
    @MediumTest
    @Features.
    EnableFeatures({PageInfoFeatureList.PAGE_INFO_V2, PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY,
            SiteSettingsFeatureList.ACTIONABLE_CONTENT_SETTINGS})
    public void
    testShowPermissionsSubpageWithHighlight() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfoWithPermission(
                mTestServerRule.getServer().getURL(sSimpleHtml), ContentSettingsType.GEOLOCATION);
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        onViewWaiting(allOf(withText("Control this site's access to your device"), isDisplayed()));
        Context context = InstrumentationRegistry.getTargetContext();
        // Find the preference and check its background color.
        onView(allOf(withParent(withId(R.id.recycler_view)),
                       hasDescendant(withText(
                               context.getString(R.string.website_settings_device_location)))))
                .check(matches(hasBackgroundColor(R.color.iph_highlight_blue)));
    }

    /**
     * Tests that a close button is shown with accessibility enabled and that it closes the dialog.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testCloseButton() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true); });
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertTrue(controller.isDialogShowingForTesting());
        onView(withId(R.id.page_info_close)).perform(click());
        assertFalse(controller.isDialogShowingForTesting());
    }

    // TODO(1071762): Add tests for preview pages, offline pages, offline state and other states.
}
