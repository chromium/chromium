// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests that exercise functionality when showing details for a single site.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(SingleWebsiteSettingsTest.TEST_BATCH_NAME)
public class SingleWebsiteSettingsTest {
    private static final String EXAMPLE_ADDRESS = "https://example.com";

    static final String TEST_BATCH_NAME = "SingleWebsiteSettingsTest";

    @ClassRule
    public static ChromeTabbedActivityTestRule sCTATestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sCTATestRule, false);

    /**
     * A provider supplying params for {@link #testExceptionToggleShowing}.
     *
     * Entries in SingleWebsiteSettings should only have Allow/Block values (independent of what the
     * global toggle specifies), because if there's a ContentSettingValue entry for the site, then
     * the user has already made a decision. That decision can only be Allow/Block (a decision of
     * ASK doesn't make sense because we don't support 'Ask me every time' for a given site).
     * */
    public static class SingleWebsiteSettingsParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            ArrayList<ParameterSet> testCases = new ArrayList<>();
            for (@ContentSettingsType int contentSettings : SiteSettingsUtil.SETTINGS_ORDER) {
                testCases.add(
                        createParameterSet("Allow_", contentSettings, ContentSettingValues.ALLOW));
                testCases.add(
                        createParameterSet("Block_", contentSettings, ContentSettingValues.BLOCK));
            }
            return testCases;
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(SingleWebsiteSettingsParams.class)
    public void testExceptionToggleShowing(@ContentSettingsType int contentSettingsType,
            @ContentSettingValues int contentSettingValue) {
        // Preference for Notification on O+ is added as a ChromeImageViewPreference. See
        // SingleWebsiteSettings#setUpNotificationsPreference
        Assume.assumeFalse("Preference for Notification is not a toggle on Android N-.",
                contentSettingsType == ContentSettingsType.NOTIFICATIONS
                        && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O);

        new SingleExceptionTestCase(contentSettingsType, contentSettingValue).run();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "Notification does not have a toggle when disabled.")
    // clang-format off
    public void testNotificationException() {
        // clang-format on
        SettingsActivity settingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(
                createWebsiteWithContentSettingException(
                        ContentSettingsType.NOTIFICATIONS, ContentSettingValues.BLOCK));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();
            Assert.assertNotNull("Notification Preference not found.",
                    websitePreferences.findPreference(SingleWebsiteSettings.getPreferenceKey(
                            ContentSettingsType.NOTIFICATIONS)));
        });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    public void testDesktopSiteException_DowngradePath() {
        // Enable RDS exceptions and launch single website settings.
        Map<String, Boolean> featureMap = new HashMap<>();
        featureMap.put(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, true);
        FeatureList.setTestFeatures(featureMap);

        SettingsActivity settingsActivity1 = SiteSettingsTestUtils.startSingleWebsitePreferences(
                createWebsiteWithContentSettingException(
                        ContentSettingsType.REQUEST_DESKTOP_SITE, ContentSettingValues.ALLOW));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            var websitePreferences = (SingleWebsiteSettings) settingsActivity1.getMainFragment();
            Assert.assertNotNull("Desktop site preference should be present.",
                    websitePreferences.findPreference(SingleWebsiteSettings.getPreferenceKey(
                            ContentSettingsType.REQUEST_DESKTOP_SITE)));
        });
        settingsActivity1.finish();

        // Disable RDS exceptions for a downgrade and launch single website settings.
        featureMap.put(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, false);
        featureMap.put(SiteSettingsFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS_DOWNGRADE, true);
        FeatureList.setTestFeatures(featureMap);

        SettingsActivity settingsActivity2 = SiteSettingsTestUtils.startSingleWebsitePreferences(
                createWebsiteWithContentSettingException(
                        ContentSettingsType.REQUEST_DESKTOP_SITE, ContentSettingValues.ALLOW));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            var websitePreferences = (SingleWebsiteSettings) settingsActivity2.getMainFragment();
            Assert.assertNull("Desktop site preference should not be present.",
                    websitePreferences.findPreference(SingleWebsiteSettings.getPreferenceKey(
                            ContentSettingsType.REQUEST_DESKTOP_SITE)));
        });
        settingsActivity2.finish();
    }

    /**
     * Helper function for creating a {@link ParameterSet} for {@link SingleWebsiteSettingsParams}.
     */
    private static ParameterSet createParameterSet(String namePrefix,
            @ContentSettingsType int contentSettingsType,
            @ContentSettingValues int contentSettingValue) {
        String prefKey = SingleWebsiteSettings.getPreferenceKey(contentSettingsType);
        Assert.assertNotNull(
                "Preference key is missing for ContentSettingsType <" + contentSettingsType + ">.",
                prefKey);

        return new ParameterSet()
                .name(namePrefix + prefKey)
                .value(contentSettingsType, contentSettingValue);
    }

    /** Test case class that check whether a toggle exists for a given content setting. */
    private static class SingleExceptionTestCase {
        @ContentSettingValues
        int mContentSettingValue;
        @ContentSettingsType
        int mContentSettingsType;

        private SettingsActivity mSettingsActivity;

        SingleExceptionTestCase(@ContentSettingsType int contentSettingsType,
                @ContentSettingValues int contentSettingValue) {
            mContentSettingsType = contentSettingsType;
            mContentSettingValue = contentSettingValue;
        }

        public void run() {
            Website website = createWebsiteWithContentSettingException(
                    mContentSettingsType, mContentSettingValue);
            mSettingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SingleWebsiteSettings websitePreferences =
                        (SingleWebsiteSettings) mSettingsActivity.getMainFragment();
                doTest(websitePreferences);
            });

            mSettingsActivity.finish();
        }

        protected void doTest(SingleWebsiteSettings websitePreferences) {
            String prefKey = SingleWebsiteSettings.getPreferenceKey(mContentSettingsType);
            ChromeSwitchPreference switchPref = websitePreferences.findPreference(prefKey);
            Assert.assertNotNull("Preference cannot be found on screen.", switchPref);
            Assert.assertEquals("Switch check state is different than test setting.",
                    mContentSettingValue == ContentSettingValues.ALLOW, switchPref.isChecked());
        }
    }

    private static Website createWebsiteWithContentSettingException(
            @ContentSettingsType int type, @ContentSettingValues int value) {
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        website.setContentSettingException(type,
                new ContentSettingException(type, website.getAddress().getOrigin(), value,
                        "preference", /*isEmbargoed=*/false));

        return website;
    }
}
