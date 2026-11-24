// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.site_settings.CookiesInfo;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.List;

/**
 * Instrumentation tests for the RestrictYouTubeCookiesDeletion enterprise policy in Android Site
 * Settings UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@NullMarked
public class RestrictYouTubeCookiesDeletionPolicyTest {
    private static final String POLICY_NAME = "RestrictYouTubeCookiesDeletion";
    private static final String YOUTUBE_DOMAIN = "youtube.com";
    private static final String YOUTUBE_ORIGIN = "https://www.youtube.com";
    private static final String MOBILE_YOUTUBE_ORIGIN = "https://m.youtube.com";
    private static final String EXAMPLE_ORIGIN = "https://www.example.com";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @MediumTest
    @Feature({"Policy", "Preferences"})
    public void testPolicyUnset() {
        verifyPolicyAndSiteSettings(
                /* expectedManaged= */ false, /* expectedDeletionDisabled= */ false);
    }

    @Test
    @MediumTest
    @Feature({"Policy", "Preferences"})
    @Policies.Add({@Policies.Item(key = POLICY_NAME, string = "true")})
    public void testPolicyTrue() {
        verifyPolicyAndSiteSettings(
                /* expectedManaged= */ true, /* expectedDeletionDisabled= */ true);
    }

    @Test
    @MediumTest
    @Feature({"Policy", "Preferences"})
    @Policies.Add({@Policies.Item(key = POLICY_NAME, string = "false")})
    public void testPolicyFalse() {
        verifyPolicyAndSiteSettings(
                /* expectedManaged= */ true, /* expectedDeletionDisabled= */ false);
    }

    private void verifyPolicyAndSiteSettings(
            boolean expectedManaged, boolean expectedDeletionDisabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    PrefService prefs = UserPrefs.get(profile);
                    Assert.assertEquals(
                            expectedManaged,
                            prefs.isManagedPreference(Pref.RESTRICT_YOU_TUBE_COOKIES_DELETION));
                    Assert.assertEquals(
                            expectedDeletionDisabled,
                            prefs.getBoolean(Pref.RESTRICT_YOU_TUBE_COOKIES_DELETION));
                    Assert.assertEquals(
                            expectedDeletionDisabled,
                            WebsitePreferenceBridge.isCookieDeletionDisabled(
                                    profile, YOUTUBE_ORIGIN));
                    Assert.assertEquals(
                            expectedDeletionDisabled,
                            WebsitePreferenceBridge.isCookieDeletionDisabled(
                                    profile, MOBILE_YOUTUBE_ORIGIN));
                    Assert.assertFalse(
                            WebsitePreferenceBridge.isCookieDeletionDisabled(
                                    profile, EXAMPLE_ORIGIN));
                });

        verifySingleWebsiteSettings(YOUTUBE_ORIGIN, expectedDeletionDisabled);
        verifySingleWebsiteSettings(EXAMPLE_ORIGIN, /* expectedDeletionDisabled= */ false);
        verifyGroupedWebsitesSettings(expectedDeletionDisabled);
    }

    private void verifySingleWebsiteSettings(String origin, boolean expectedDeletionDisabled) {
        SettingsActivity activity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(
                        createWebsiteWithCookies(origin, /* count= */ 1));
        waitForView(withText(origin));
        waitForView(
                allOf(withId(R.id.button_preference), withText(R.string.website_reset_full)),
                expectedDeletionDisabled
                        ? ViewElement.expectDisabledOption()
                        : ViewElement.defaultOptions());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleWebsiteSettings fragment =
                            (SingleWebsiteSettings) activity.getMainFragment();
                    Assert.assertNotNull(fragment);
                    assumeNonNull(fragment);
                    Preference clearDataPref =
                            fragment.findPreference(SingleWebsiteSettings.PREF_CLEAR_DATA);
                    Preference resetSitePref =
                            fragment.findPreference(SingleWebsiteSettings.PREF_RESET_SITE);
                    Assert.assertNotNull(clearDataPref);
                    Assert.assertNotNull(resetSitePref);
                    assumeNonNull(clearDataPref);
                    assumeNonNull(resetSitePref);
                    Assert.assertEquals(!expectedDeletionDisabled, clearDataPref.isEnabled());
                    Assert.assertEquals(!expectedDeletionDisabled, resetSitePref.isEnabled());
                });
        ApplicationTestUtils.finishActivity(activity);
    }

    private void verifyGroupedWebsitesSettings(boolean expectedDeletionDisabled) {
        WebsiteGroup youtubeGroup =
                new WebsiteGroup(
                        YOUTUBE_DOMAIN,
                        List.of(
                                createWebsiteWithCookies(YOUTUBE_ORIGIN, /* count= */ 1),
                                createWebsiteWithCookies(MOBILE_YOUTUBE_ORIGIN, /* count= */ 1)));
        SettingsActivity groupActivity =
                SiteSettingsTestUtils.startGroupedWebsitesPreferences(youtubeGroup);
        waitForView(withText(YOUTUBE_DOMAIN));
        waitForView(
                allOf(withId(R.id.button_preference), withText(R.string.website_reset_full)),
                (expectedDeletionDisabled
                                ? ViewElement.newOptions().expectDisabled()
                                : ViewElement.newOptions())
                        .displayingAtLeast(1)
                        .build());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GroupedWebsitesSettings fragment =
                            (GroupedWebsitesSettings) groupActivity.getMainFragment();
                    Assert.assertNotNull(fragment);
                    assumeNonNull(fragment);
                    Preference clearDataPref =
                            fragment.findPreference(GroupedWebsitesSettings.PREF_CLEAR_DATA);
                    Preference resetGroupPref =
                            fragment.findPreference(GroupedWebsitesSettings.PREF_RESET_GROUP);
                    Assert.assertNotNull(clearDataPref);
                    Assert.assertNotNull(resetGroupPref);
                    assumeNonNull(clearDataPref);
                    assumeNonNull(resetGroupPref);
                    Assert.assertEquals(!expectedDeletionDisabled, clearDataPref.isEnabled());
                    Assert.assertEquals(!expectedDeletionDisabled, resetGroupPref.isEnabled());
                });
        ApplicationTestUtils.finishActivity(groupActivity);
    }

    private static Website createWebsiteWithCookies(String origin, int count) {
        WebsiteAddress address = assumeNonNull(WebsiteAddress.create(origin));
        Website site = new Website(address, address);
        site.setCookiesInfo(new CookiesInfo(count));
        return site;
    }
}
