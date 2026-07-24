// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.site_settings.RwsCookieInfo;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Render tests for Site Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DisableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
public class SiteSettingsRenderTest {

    private static void createAndSetRwsCookieInfo(Website owner, List<Website> websiteList) {
        RwsCookieInfo rwsInfo = new RwsCookieInfo(owner.getAddress().getOrigin(), websiteList);
        owner.setRwsCookieInfo(rwsInfo);
    }

    private static Website getRwsOwnerSite() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        createAndSetRwsCookieInfo(origin1, List.of(origin1, origin2));
        return origin1;
    }

    private static WebsiteGroup getRwsSiteGroup() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        createAndSetRwsCookieInfo(origin1, List.of(origin1, origin2));
        return new WebsiteGroup(origin1.getAddress().getOrigin(), Arrays.asList(origin1, origin2));
    }

    private static final int RENDER_TEST_REVISION = 6;

    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    public PermissionTestRule mPermissionTestRule =
            new PermissionTestRule(mActivityTestRule.getActivityTestRule(), true);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    public AdvancedProtectionTestRule mAdvancedProtectionRule = new AdvancedProtectionTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAdvancedProtectionRule)
                    .around(mActivityTestRule)
                    .around(mPermissionTestRule);

    @Before
    public void setUp() throws TimeoutException {
        try {
            SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
        } catch (TimeoutException e) {
            // Sometimes there's a callback timeout here. Doesn't seem to impact test results.
        }
    }

    @After
    public void tearDown() throws Exception {
        SiteSettingsTestHelper.cleanUpContentSettingsAndExceptions(new CallbackHelper());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderStorageAccessPage() throws Exception {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.STORAGE_ACCESS, "site_settings_storage_access_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderStorageAccessSubpage() throws Exception {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startStorageAccessSettingsActivity(
                        SiteSettingsTestHelper.getStorageAccessSite());
        renderSettingsPage(settingsActivity, "site_settings_storage_access_subpage");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    @DisabledTest(message = "crbug.com/468373452")
    public void renderRwsSingleWebsiteSettings() throws Exception {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(getRwsOwnerSite());
        renderSettingsPage(settingsActivity, "site_settings_rws_single_website");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderRwsGroupedWebsiteSettings() throws Exception {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startGroupedWebsitesPreferences(getRwsSiteGroup());
        renderSettingsPage(settingsActivity, "site_settings_rws_grouped_website");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderSiteDataPage() throws Exception {
        SiteSettingsTestHelper.createCookieExceptions();
        renderCategoryPage(SiteSettingsCategory.Type.SITE_DATA, "site_settings_site_data_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderThirdPartyCookiesPageWithFps() throws Exception {
        SiteSettingsTestHelper.createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Policies.Add({
        @Policies.Item(key = "BlockThirdPartyCookies", string = "true"),
        @Policies.Item(key = "RelatedWebsiteSetsEnabled", string = "true")
    })
    public void renderThirdPartyCookiesPageManagedBlocked() throws Exception {
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page_managed_blocked");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Policies.Add({@Policies.Item(key = "BlockThirdPartyCookies", string = "false")})
    public void renderThirdPartyCookiesPageManagedAllowed() throws Exception {
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page_managed_allowed");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderCookiesPageWithFps() throws Exception {
        SiteSettingsTestHelper.createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, "site_settings_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    public void testRenderLocationPage() throws Exception {
        SiteSettingsTestHelper.createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.DEVICE_LOCATION, "site_settings_location_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderProtectedMediaPage() throws Exception {
        SiteSettingsTestHelper.createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, "site_settings_protected_media_page");
    }

    private void renderCategoryPage(@SiteSettingsCategory.Type int category, String name)
            throws IOException {
        var settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(category);
        renderSettingsPage(settingsActivity, name);
    }

    private void renderSettingsPage(SettingsActivity settingsActivity, String name)
            throws IOException {
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, name);
        settingsActivity.finish();
    }
}
