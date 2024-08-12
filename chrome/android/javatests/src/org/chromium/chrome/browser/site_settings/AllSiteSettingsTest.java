// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.Set;
import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(AllSiteSettingsTest.TEST_BATCH_NAME)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class AllSiteSettingsTest {
    public static final String TEST_BATCH_NAME = "AllSiteSettingsTest";
    private static final String A_GITHUB_IO = "a.github.io";
    private static final String B_GITHUB_IO = "b.github.io";
    private static final String C_GITHUB_IO = "c.github.io";
    private static final String D_GITHUB_IO = "d.github.io";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @ClassRule public static PermissionTestRule mPermissionRule = new PermissionTestRule(true);

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mPermissionRule, false);

    @Rule public JniMocker mocker = new JniMocker();

    private static BrowserContextHandle getBrowserContextHandle() {
        return ProfileManager.getLastUsedRegularProfile();
    }

    @Before
    public void setUp() throws TimeoutException {
        SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "RenderTest"})
    public void testAllSitesViewEmpty() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceFragmentCompat preferenceFragment =
                                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                            return preferenceFragment.getView();
                        });
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "site_settings_all_sites_empty");
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "RenderTest"})
    public void testAllSitesViewSingleDomain() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "google.com",
                            "*",
                            ContentSettingValues.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceFragmentCompat preferenceFragment =
                                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                            return preferenceFragment.getView();
                        });
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "site_settings_all_sites_single_domain");
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllSitesUsePublicSuffixList() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            A_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            B_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        onView(withText(A_GITHUB_IO)).check(matches(isDisplayed()));
        onView(withText(B_GITHUB_IO)).check(matches(isDisplayed()));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllSitesWithRelatedFilter() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            A_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            B_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                });
        String relatedFilter = String.format("related:%s", C_GITHUB_IO);

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettingsForRws(
                        SiteSettingsCategory.Type.ALL_SITES, C_GITHUB_IO);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        onView(withText(relatedFilter)).check(matches(isDisplayed()));
        onView(withText(A_GITHUB_IO)).check(doesNotExist());
        onView(withText(B_GITHUB_IO)).check(doesNotExist());

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_UI)
    public void testOneRwsGroupWithRelatedFilter() throws Exception {
        FakeRwsPrivacySandboxBridge fakeRwsPrivacySandboxBridge =
                new FakeRwsPrivacySandboxBridge(C_GITHUB_IO, Set.of(A_GITHUB_IO, B_GITHUB_IO));
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, fakeRwsPrivacySandboxBridge);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            A_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            B_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            D_GITHUB_IO,
                            "*",
                            ContentSettingValues.ALLOW);
                });
        String relatedFilter = String.format("related:%s", C_GITHUB_IO);

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettingsForRws(
                        SiteSettingsCategory.Type.ALL_SITES, C_GITHUB_IO);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));

        onView(withText(relatedFilter)).check(matches(isDisplayed()));
        onView(withText(A_GITHUB_IO)).check(matches(isDisplayed()));
        onView(withText(B_GITHUB_IO)).check(matches(isDisplayed()));
        onView(withText(D_GITHUB_IO)).check(doesNotExist());

        settingsActivity.finish();
    }
}
