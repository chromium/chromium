// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different
 * configurations correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
public class PageInfoViewTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(3).build();

    private boolean mIsSystemLocationSettingEnabled = true;

    private class TestLocationUtils extends LocationUtils {
        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mIsSystemLocationSettingEnabled;
        }
    }

    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    private final String mPath = "/chrome/test/data/android/simple.html";

    private void loadUrlAndOpenPageInfo(String url) {
        mActivityTestRule.loadUrl(url);
        onView(withId(R.id.location_bar_status_icon)).perform(click());
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

    private void addSomePermissions(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.GEOLOCATION, url, "*", ContentSettingValues.ALLOW);
            WebsitePreferenceBridge.setContentSettingForPattern(Profile.getLastUsedRegularProfile(),
                    ContentSettingsType.NOTIFICATIONS, url, "*", ContentSettingValues.ALLOW);
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

    @Before
    public void setUp() throws InterruptedException {
        // Some test devices have geolocation disabled. Override LocationUtils for a stable result.
        LocationUtils.setFactory(TestLocationUtils::new);

        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        LocationUtils.setFactory(null);
        // Notification channels don't get cleaned up automatically.
        // TODO(crbug.com/951402): Find a general solution to avoid leaking channels between tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SiteChannelsManager manager = SiteChannelsManager.getInstance();
                manager.deleteAllSiteChannels();
            });
        }
    }

    /**
     * Tests PageInfo on an insecure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnInsecureHttpWebsite() throws IOException {
        mTestServerRule.setServerUsesHttps(false);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_HttpWebsite");
    }

    /**
     * Tests PageInfo on a secure website.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnSecureWebsite() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsite");
    }

    /**
     * Tests PageInfo on a website with expired certificate.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowOnExpiredCertificateWebsite() throws IOException {
        mTestServerRule.setCertificateType(ServerCertificate.CERT_EXPIRED);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ExpiredCertWebsite");
    }

    /**
     * Tests PageInfo on internal page.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
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
    public void testShowWithPermissions() throws IOException {
        mIsSystemLocationSettingEnabled = false;
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_Permissions");
    }

    /**
     * Tests PageInfo on a website with cookie controls enabled.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithCookieBlocking() throws IOException {
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookieBlocking");
    }

    /**
     * Tests PageInfo on a website with cookie controls and permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithPermissionsAndCookieBlocking() throws IOException {
        addSomePermissions(mTestServerRule.getServer().getURL("/"));
        setThirdPartyCookieBlocking(CookieControlsMode.BLOCK_THIRD_PARTY);
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsAndCookieBlocking");
    }

    /**
     * Tests PageInfo on a website with default setting permissions.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowWithDefaultSettingPermissions() throws IOException {
        addDefaultSettingPermissions(mTestServerRule.getServer().getURL("/"));
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
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
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
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
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        onView(withId(R.id.page_info_connection_row)).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_ConnectionInfoSubpage");
    }

    /**
     * Tests that the permissions page of the new PageInfo UI is gone when there are no permissions
     * set.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testNoPermissionsSubpage() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        View dialog = (View) getPageInfoView().getParent();
        onView(withId(R.id.page_info_permissions_row))
                .check(matches(withEffectiveVisibility(GONE)));
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
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        onView(withId(R.id.page_info_permissions_row)).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_PermissionsSubpage");
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
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(mPath));
        onView(withId(R.id.page_info_cookies_row)).perform(click());
        mRenderTestRule.render(getPageInfoView(), "PageInfo_CookiesSubpage");
    }

    // TODO(1071762): Add tests for preview pages, offline pages, offline state and other states.
}
