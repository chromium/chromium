// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertNotNull;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.junit.Assert;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.BinaryStatePermissionPreference;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.CookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.TriStateSiteSettingsPreference;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Common utility and helper methods across batched SiteSettings instrumentation test suites. */
public class SiteSettingsTestHelper {
    public static final String[] BINARY_RADIO_BUTTON = new String[] {"binary_radio_button"};
    public static final String[] BINARY_RADIO_BUTTON_WITH_EXCEPTION =
            new String[] {"binary_radio_button", "add_exception"};
    public static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA =
            new String[] {"binary_radio_button", "os_permissions_warning_extra"};
    public static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "os_permissions_warning_extra"};
    public static final String[] CLEAR_BROWSING_DATA_LINK =
            new String[] {"clear_browsing_data_link", "clear_browsing_divider"};

    public static BrowserContextHandle getBrowserContextHandle() {
        return ProfileManager.getLastUsedRegularProfile();
    }

    public static void cleanUpRunningActivities() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Activity activity : ApplicationStatus.getRunningActivities()) {
                        if (!(activity instanceof ChromeTabbedActivity)
                                && !activity.isFinishing()) {
                            activity.finish();
                        }
                    }
                });
    }

    public static void cleanUpContentSettingsAndExceptions(CallbackHelper helper)
            throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Clean up default content setting and system settings.
                    for (int t = 0; t < SiteSettingsCategory.Type.NUM_ENTRIES; t++) {
                        if (SiteSettingsCategory.contentSettingsType(t) >= 0) {
                            WebsitePreferenceBridge.setDefaultContentSetting(
                                    getBrowserContextHandle(),
                                    SiteSettingsCategory.contentSettingsType(t),
                                    ContentSetting.DEFAULT);
                        }
                    }
                    // Clean up content setting exceptions.
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
        cleanUpRunningActivities();
    }

    public static void createCookieExceptions() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "*",
                            "secondary.com",
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "primary.com",
                            "*",
                            ContentSetting.ALLOW);
                });
    }

    public static void createStorageAccessExceptions() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.STORAGE_ACCESS,
                            "primary.com",
                            "secondary.com",
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.STORAGE_ACCESS,
                            "primary.com",
                            "secondary3.com",
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.STORAGE_ACCESS,
                            "primary2.com",
                            "secondary2.com",
                            ContentSetting.ALLOW);
                });
    }

    public static Website getStorageAccessSite() {
        WebsiteAddress permissionOrigin = WebsiteAddress.create("primary.com");
        WebsiteAddress permissionEmbedder = WebsiteAddress.create("*");
        Website site = new Website(permissionOrigin, permissionEmbedder);
        site.addEmbeddedPermission(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        "primary.com",
                        "secondary1.com",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        30,
                        false));
        site.addEmbeddedPermission(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        "primary.com",
                        "secondary3.com",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        30,
                        false));

        return site;
    }

    public static void resetSite(WebsiteAddress address) {
        Website website = new Website(address, address);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    websitePreferences.resetSite();
                });
        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    public static String getChannelId(String url) {
        PayloadCallbackHelper<String> helper = new PayloadCallbackHelper<>();
        SiteChannelsManager.getInstance()
                .getChannelIdForOriginAsync(
                        Origin.createOrThrow(url).toString(), helper::notifyCalled);
        return helper.getOnlyPayloadBlocking();
    }

    public static void setGlobalTriStateToggleForCategory(
            final @SiteSettingsCategory.Type int type, final int newValue) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    TriStateSiteSettingsPreference triStateToggle =
                            preferences.findPreference(SingleCategorySettings.TRI_STATE_TOGGLE_KEY);
                    preferences.onPreferenceChange(triStateToggle, newValue);
                });
        settingsActivity.finish();
    }

    /**
     * Tests that the Preferences designated by keys in |expectedKeys|, and only these preferences,
     * will be shown for the category specified by |type|. The order of Preferences matters.
     */
    public static void checkPreferencesForCategory(
            final @SiteSettingsCategory.Type int type, String[] expectedKeys) {
        final SettingsActivity settingsActivity;

        if (type == SiteSettingsCategory.Type.ALL_SITES
                || type == SiteSettingsCategory.Type.USE_STORAGE
                || type == SiteSettingsCategory.Type.ZOOM) {
            settingsActivity = SiteSettingsTestUtils.startAllSitesSettings(type);
        } else {
            settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(type);
        }

        checkPreferencesForSettingsActivity(settingsActivity, expectedKeys);
        settingsActivity.finish();
    }

    public static void checkPreferencesForSettingsActivity(
            SettingsActivity settingsActivity, String[] expectedKeys) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceFragmentCompat preferenceFragment =
                            (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                    PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
                    int preferenceCount = preferenceScreen.getPreferenceCount();

                    ArrayList<String> actualKeys = new ArrayList<>();
                    for (int index = 0; index < preferenceCount; index++) {
                        Preference preference = preferenceScreen.getPreference(index);
                        if (!preference.isVisible()) continue;
                        String key = preference.getKey();
                        // Not all Preferences have keys. For example, the list of websites below
                        // the toggles, which are dynamically added. Ignore those.
                        if (key != null) actualKeys.add(key);
                    }

                    assertThat(
                            actualKeys,
                            expectedKeys.length == 0 ? emptyIterable() : contains(expectedKeys));
                });
    }

    public static void testExpectedPreferences(
            final @SiteSettingsCategory.Type int type,
            String[] disabledExpectedKeys,
            String[] enabledExpectedKeys) {
        // Disable the category and check for the right preferences.
        setGlobalToggleForCategory(type, false);
        checkPreferencesForCategory(type, disabledExpectedKeys);
        // Re-enable the category and check for the right preferences.
        setGlobalToggleForCategory(type, true);
        checkPreferencesForCategory(type, enabledExpectedKeys);
    }

    public static class PermissionTestCase {
        protected final String mTestName;
        protected final @SiteSettingsCategory.Type int mSiteSettingsType;
        protected final @ContentSettingsType.EnumType int mContentSettingsType;
        protected final boolean mIsCategoryEnabled;
        protected final List<String> mExpectedPreferenceKeys;

        protected SettingsActivity mSettingsActivity;

        public PermissionTestCase(
                final String testName,
                @SiteSettingsCategory.Type final int siteSettingsType,
                @ContentSettingsType.EnumType final int contentSettingsType,
                final boolean enabled) {
            mTestName = testName;
            mSiteSettingsType = siteSettingsType;
            mContentSettingsType = contentSettingsType;
            mIsCategoryEnabled = enabled;

            mExpectedPreferenceKeys = new ArrayList<>();
        }

        /** Set extra expected pref keys for category settings screen. */
        public PermissionTestCase withExpectedPrefKeys(String expectedPrefKeys) {
            mExpectedPreferenceKeys.add(expectedPrefKeys);
            return this;
        }

        public PermissionTestCase withExpectedPrefKeysAtStart(String expectedPrefKeys) {
            mExpectedPreferenceKeys.add(0, expectedPrefKeys);
            return this;
        }

        public PermissionTestCase withExpectedPrefKeys(String[] expectedPrefKeys) {
            mExpectedPreferenceKeys.addAll(Arrays.asList(expectedPrefKeys));
            return this;
        }

        public void run() {
            mSettingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(mSiteSettingsType);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SingleCategorySettings singleCategorySettings =
                                (SingleCategorySettings) mSettingsActivity.getMainFragment();

                        doTest(singleCategorySettings);
                    });
            mSettingsActivity.finish();
        }

        protected void doTest(SingleCategorySettings singleCategorySettings) {
            assertPreferenceOnScreen(singleCategorySettings, mExpectedPreferenceKeys);
        }

        protected void assertPreferenceOnScreen(
                SingleCategorySettings singleCategorySettings, List<String> expectedKeys) {
            PreferenceScreen preferenceScreen = singleCategorySettings.getPreferenceScreen();
            int preferenceCount = preferenceScreen.getPreferenceCount();

            ArrayList<String> actualKeys = new ArrayList<>();
            for (int index = 0; index < preferenceCount; index++) {
                Preference preference = preferenceScreen.getPreference(index);
                String key = preference.getKey();
                // Not all Preferences have keys. For example, the list of websites below the
                // toggles, which are dynamically added. Ignore those.
                if (key != null && preference.isVisible()) actualKeys.add(key);
            }

            Assert.assertEquals(
                    actualKeys.toString() + " should match " + expectedKeys.toString(),
                    expectedKeys,
                    actualKeys);
        }
    }

    /** Test case for site settings with a global radio button group. */
    public static class TwoStatePermissionTestCaseWithRadioButton extends PermissionTestCase {
        public TwoStatePermissionTestCaseWithRadioButton(
                String testName, int siteSettingsType, int contentSettingsType, boolean enabled) {
            super(testName, siteSettingsType, contentSettingsType, enabled);
            mExpectedPreferenceKeys.add(SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
        }

        @Override
        public void doTest(SingleCategorySettings singleCategorySettings) {
            // Verify toggle related checks first as they may affect the preferences on the screen.
            assertRadioButtonTitleAndSummary(singleCategorySettings);
            assertGlobalRadioButtonGroupForCategory(singleCategorySettings);

            super.doTest(singleCategorySettings);
        }

        /** Verify {@link SingleCategorySettings} is wired correctly. */
        private void assertGlobalRadioButtonGroupForCategory(
                SingleCategorySettings singleCategorySettings) {
            final String exceptionString =
                    "Test <"
                            + mTestName
                            + ">: Content setting category <"
                            + mContentSettingsType
                            + "> should be "
                            + (mIsCategoryEnabled ? "enabled" : "disabled")
                            + " with Site Settings <"
                            + mSiteSettingsType
                            + ">.";

            BinaryStatePermissionPreference radioButton =
                    singleCategorySettings.findPreference(
                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
            assertNotNull("Radio Button should not be null.", radioButton);

            singleCategorySettings.onPreferenceChange(radioButton, mIsCategoryEnabled);
            Assert.assertEquals(
                    exceptionString,
                    mIsCategoryEnabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), mContentSettingsType));
        }

        /** Verify {@link ContentSettingsResources} is set correctly. */
        private void assertRadioButtonTitleAndSummary(
                SingleCategorySettings singleCategorySettings) {
            BinaryStatePermissionPreference radioButton =
                    singleCategorySettings.findPreference(
                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
            Assert.assertNotNull(radioButton);

            Assert.assertEquals(
                    "Preference text is not set correctly.",
                    ContentSettingsResources.getBinaryStateSettingResourceIDs(mContentSettingsType)[
                            0],
                    radioButton.getDescriptionIds()[0]);
            Assert.assertEquals(
                    "Preference text is not set correctly.",
                    ContentSettingsResources.getBinaryStateSettingResourceIDs(mContentSettingsType)[
                            1],
                    radioButton.getDescriptionIds()[1]);
        }
    }

    public static void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
                        CookieSettingsPreference preference =
                                preferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);
                        preferences.onPreferenceChange(
                                preference,
                                enabled
                                        ? CookieControlsMode.INCOGNITO_ONLY
                                        : CookieControlsMode.BLOCK_THIRD_PARTY);
                    } else if (type != SiteSettingsCategory.Type.ANTI_ABUSE) {
                        BinaryStatePermissionPreference radioButton =
                                preferences.findPreference(
                                        SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);

                        preferences.onPreferenceChange(radioButton, enabled);
                    } else {
                        ChromeSwitchPreference toggle =
                                preferences.findPreference(
                                        SingleCategorySettings.BINARY_TOGGLE_KEY);
                        preferences.onPreferenceChange(toggle, enabled);
                    }
                });
        if (type == SiteSettingsCategory.Type.SITE_DATA && !enabled) {
            int id = R.string.website_settings_site_data_page_block_confirm_dialog_confirm_button;
            onViewWaiting(withText(id)).perform(click());
        }
        settingsActivity.finish();
    }

    public static final String[] BINARY_RADIO_BUTTON_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button"};

    public static final String[] BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "add_exception"};

    public static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "os_permissions_warning"};
}
