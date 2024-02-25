// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
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

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.privacy_sandbox.FakeTrackingProtectionBridge;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionBridgeJni;
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
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(AllSiteSettingsTest.TEST_BATCH_NAME)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class AllSiteSettingsTest {
    public static final String TEST_BATCH_NAME = "AllSiteSettingsTest";

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

    private FakeTrackingProtectionBridge mFakeTrackingProtectionBridge;

    private static BrowserContextHandle getBrowserContextHandle() {
        return ProfileManager.getLastUsedRegularProfile();
    }

    @Before
    public void setUp() throws TimeoutException {
        SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
        MockitoAnnotations.initMocks(this);
        mFakeTrackingProtectionBridge = new FakeTrackingProtectionBridge();
        mocker.mock(TrackingProtectionBridgeJni.TEST_HOOKS, mFakeTrackingProtectionBridge);
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "RenderTest"})
    public void testAllSitesViewEmpty() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Clear browsing"))).check(matches(isDisplayed()));
        View view =
                TestThreadUtils.runOnUiThreadBlocking(
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
        TestThreadUtils.runOnUiThreadBlocking(
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
        onViewWaiting(withText(containsString("Clear browsing"))).check(matches(isDisplayed()));
        View view =
                TestThreadUtils.runOnUiThreadBlocking(
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "a.github.io",
                            "*",
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "b.github.io",
                            "*",
                            ContentSettingValues.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Clear browsing"))).check(matches(isDisplayed()));
        onView(withText("a.github.io")).check(matches(isDisplayed()));
        onView(withText("b.github.io")).check(matches(isDisplayed()));

        settingsActivity.finish();
    }
}
