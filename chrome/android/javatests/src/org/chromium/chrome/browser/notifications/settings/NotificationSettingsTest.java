// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.settings;

import android.os.Build;

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchPrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the NotificationSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class NotificationSettingsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();
    @Rule
    public final SettingsActivityTestRule<NotificationSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(NotificationSettings.class);
    private SettingsActivity mActivity;

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setUp() {
        mActivity = mSettingsActivityTestRule.startSettingsActivity();
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @CommandLineFlags.Add("enable-features=OfflinePagesPrefetching")
    public void testContentSuggestionsToggle() {
        // clang-format on

        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final ChromeSwitchPreference toggle = (ChromeSwitchPreference) fragment.findPreference(
                NotificationSettings.PREF_SUGGESTIONS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Make sure the toggle reflects the state correctly.
            boolean initiallyChecked = toggle.isChecked();
            Assert.assertEquals(toggle.isChecked(), PrefetchPrefs.getNotificationEnabled());

            // Make sure we can change the state.
            toggle.performClick();
            Assert.assertEquals(toggle.isChecked(), !initiallyChecked);
            Assert.assertEquals(toggle.isChecked(), PrefetchPrefs.getNotificationEnabled());

            // Make sure we can change it back.
            toggle.performClick();
            Assert.assertEquals(toggle.isChecked(), initiallyChecked);
            Assert.assertEquals(toggle.isChecked(), PrefetchPrefs.getNotificationEnabled());

            // Click it one last time so we're in a toggled state for the UI Capture.
            toggle.performClick();
        });

        mScreenShooter.shoot("ContentSuggestionsToggle");
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @CommandLineFlags.Add("disable-features=OfflinePagesPrefetching")
    public void testToggleDisabledWhenPrefetchingDisabled() {
        // clang-format on

        PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        ChromeSwitchPreference toggle = (ChromeSwitchPreference) fragment.findPreference(
                NotificationSettings.PREF_SUGGESTIONS);

        Assert.assertFalse(toggle.isEnabled());
        Assert.assertFalse(toggle.isChecked());

        mScreenShooter.shoot("ToggleDisabledWhenSuggestionsDisabled");
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    public void testLinkToWebsiteNotifications() {
        // clang-format on

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
            Preference fromWebsites =
                    fragment.findPreference(NotificationSettings.PREF_FROM_WEBSITES);

            fromWebsites.performClick();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(getTopFragment(), Matchers.instanceOf(SingleCategorySettings.class));
        });

        SingleCategorySettings fragment = (SingleCategorySettings) getTopFragment();
        Assert.assertTrue(
                fragment.getCategoryForTest().showSites(SiteSettingsCategory.Type.NOTIFICATIONS));

        mScreenShooter.shoot("LinkToWebsiteNotifications");
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    public void testWebsiteNotificationsSummary() {
        // clang-format on

        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final Preference fromWebsites =
                fragment.findPreference(NotificationSettings.PREF_FROM_WEBSITES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setCategoryEnabled(
                    Profile.getLastUsedRegularProfile(), ContentSettingsType.NOTIFICATIONS, false);
            fragment.onResume();
            Assert.assertEquals(fromWebsites.getSummary(), getNotificationsSummary(false));

            WebsitePreferenceBridge.setCategoryEnabled(
                    Profile.getLastUsedRegularProfile(), ContentSettingsType.NOTIFICATIONS, true);
            fragment.onResume();
            Assert.assertEquals(fromWebsites.getSummary(), getNotificationsSummary(true));
        });
    }

    /**
     * Gets the fragment of the top Activity. Assumes the top Activity is a {@link
     * SettingsActivity}.
     */
    private static Fragment getTopFragment() {
        SettingsActivity settingsActivity =
                (SettingsActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        return settingsActivity.getMainFragment();
    }

    /** Gets the summary text that should be used for site specific notifications. */
    private String getNotificationsSummary(boolean enabled) {
        return mActivity.getResources().getString(ContentSettingsResources.getCategorySummary(
                ContentSettingsType.NOTIFICATIONS, enabled));
    }
}
