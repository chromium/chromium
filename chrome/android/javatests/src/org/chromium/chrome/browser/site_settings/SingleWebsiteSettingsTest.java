// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.ChosenObjectInfo;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.FileEditingInfo;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Tests that exercise functionality when showing details for a single site. */
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

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    /** A provider supplying params for {@link #testExceptionToggleShowing}. */
    public static class SingleWebsiteSettingsParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            ArrayList<ParameterSet> testCases = new ArrayList<>();
            for (@ContentSettingsType.EnumType
            int contentSettings : SiteSettingsUtil.SETTINGS_ORDER) {
                int enabled = SingleWebsiteSettings.getEnabledValue(contentSettings);
                testCases.add(createParameterSet("Enabled_", contentSettings, enabled));
                testCases.add(
                        createParameterSet("Block_", contentSettings, ContentSettingValues.BLOCK));
            }
            return testCases;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    @UseMethodParameter(SingleWebsiteSettingsParams.class)
    public void testExceptionToggleShowing(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSettingValues int contentSettingValue) {
        // Preference for Notification on O+ is added as a ChromeImageViewPreference. See
        // SingleWebsiteSettings#setUpNotificationsPreference
        Assume.assumeFalse(
                "Preference for Notification is not a toggle on Android N-.",
                contentSettingsType == ContentSettingsType.NOTIFICATIONS
                        && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O);

        new SingleExceptionTestCase(contentSettingsType, contentSettingValue).run();
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            sdk_is_less_than = Build.VERSION_CODES.O,
            message = "Notification does not have a toggle when disabled.")
    public void testNotificationException() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(
                        createWebsiteWithContentSettingException(
                                ContentSettingsType.NOTIFICATIONS, ContentSettingValues.BLOCK));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    Assert.assertNotNull(
                            "Notification Preference not found.",
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.NOTIFICATIONS)));
                });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    public void testDesktopSiteException() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(
                        createWebsiteWithContentSettingException(
                                ContentSettingsType.REQUEST_DESKTOP_SITE,
                                ContentSettingValues.ALLOW));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    Assert.assertNotNull(
                            "Desktop site preference should be present.",
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.REQUEST_DESKTOP_SITE)));
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    public void testChoosenObjectPermission() {
        String origin = "https://example.com";
        Website website = new Website(WebsiteAddress.create(origin), WebsiteAddress.create(origin));
        String object =
                """
                 {"name": "Some device",
                  "ephemeral-guid": "1",
                  "product-id": "2",
                  "serial-number": "3"}""";
        website.addChosenObjectInfo(
                new ChosenObjectInfo(
                        ContentSettingsType.USB_CHOOSER_DATA,
                        origin,
                        "Some device",
                        object,
                        /* isManaged= */ false));
        website.addChosenObjectInfo(
                new ChosenObjectInfo(
                        ContentSettingsType.USB_CHOOSER_DATA,
                        origin,
                        "A managed device",
                        "not needed",
                        /* isManaged= */ true));

        // Open site settings and check that permissions are displayed.
        SettingsActivity activity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        onView(withText("Some device")).check(matches(isDisplayed()));
        onView(withText("A managed device")).check(matches(isDisplayed()));

        // Reset permission and check that only the non-managed permission is removed.
        onView(withText(containsString("reset"))).perform(click());
        onView(withText("Delete & reset")).perform(click());
        onView(withText("Some device")).check(doesNotExist());
        onView(withText("A managed device")).check(matches(isDisplayed()));
        activity.finish();
    }

    @Test
    @SmallTest
    public void testFileEditingGrants() {
        when(mSiteSettingsDelegate.getFileSystemAccessGrants(EXAMPLE_ADDRESS))
                .thenReturn(new String[][] {{"path1"}, {"display1"}});
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        website.setFileEditingInfo(new FileEditingInfo(mSiteSettingsDelegate, EXAMPLE_ADDRESS));

        // Open site settings and check that the file edit grant is shown.
        SettingsActivity activity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        onView(withText("Files this site can view or edit")).check(matches(isDisplayed()));
        onView(withText("display1")).check(matches(isDisplayed()));

        // Click trash icon to remove grant and check grant and header are removed.
        when(mSiteSettingsDelegate.getFileSystemAccessGrants(EXAMPLE_ADDRESS))
                .thenReturn(new String[][] {{}, {}});
        onView(withContentDescription("Delete file editing grant? display1")).perform(click());
        onView(withText("Files this site can view or edit")).check(doesNotExist());
        onView(withText("display1")).check(doesNotExist());
        activity.finish();
    }

    @Test
    @SmallTest
    public void testStorageAccessPermission() {
        int type = ContentSettingsType.STORAGE_ACCESS;
        GURL example = new GURL("https://example.com");
        GURL embedded2 = new GURL("https://embedded2.com");

        Website website =
                createWebsiteWithStorageAccessPermission(
                        "https://[*.]embedded.com",
                        "https://[*.]example.com",
                        ContentSettingValues.ALLOW);
        Website website2 =
                createWebsiteWithStorageAccessPermission(
                        "https://[*.]embedded2.com",
                        "https://[*.]example.com",
                        ContentSettingValues.BLOCK);
        Website other =
                createWebsiteWithStorageAccessPermission(
                        "https://[*.]embedded.com",
                        "https://[*.]foo.com",
                        ContentSettingValues.BLOCK);
        Website merged =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        WebsiteAddress.create(EXAMPLE_ADDRESS), List.of(website, website2, other));

        var exceptions = merged.getEmbeddedPermissions().get(type);
        assertThat(exceptions.size()).isEqualTo(2);
        assertThat(exceptions.get(0).getContentSetting()).isEqualTo(ContentSettingValues.ALLOW);
        assertThat(exceptions.get(1).getContentSetting()).isEqualTo(ContentSettingValues.BLOCK);
        assertEquals(ContentSettingValues.BLOCK, getStorageAccessSetting(type, embedded2, example));

        // Open site settings.
        SettingsActivity activity = SiteSettingsTestUtils.startSingleWebsitePreferences(merged);
        onView(withText("embedded.com allowed")).check(matches(isDisplayed()));

        // Toggle the second permission.
        onView(withText("embedded2.com blocked")).check(matches(isDisplayed())).perform(click());
        assertEquals(ContentSettingValues.ALLOW, getStorageAccessSetting(type, embedded2, example));

        // Reset permission.
        onView(withText(containsString("reset"))).perform(click());
        onView(withText("Delete & reset")).perform(click());
        onView(withText("Embedded content")).check(doesNotExist());
        assertEquals(ContentSettingValues.ASK, getStorageAccessSetting(type, embedded2, example));
        activity.finish();
    }

    private static int getStorageAccessSetting(
            @ContentSettingsType.EnumType int contentSettingType,
            GURL primaryUrl,
            GURL secondaryUrl) {
        int[] result = {0};
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    result[0] =
                            WebsitePreferenceBridge.getContentSetting(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    contentSettingType,
                                    primaryUrl,
                                    secondaryUrl);
                });
        return result[0];
    }

    /**
     * Helper function for creating a {@link ParameterSet} for {@link SingleWebsiteSettingsParams}.
     */
    private static ParameterSet createParameterSet(
            String namePrefix,
            @ContentSettingsType.EnumType int contentSettingsType,
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
        @ContentSettingValues int mContentSettingValue;
        @ContentSettingsType.EnumType int mContentSettingsType;

        private SettingsActivity mSettingsActivity;

        SingleExceptionTestCase(
                @ContentSettingsType.EnumType int contentSettingsType,
                @ContentSettingValues int contentSettingValue) {
            mContentSettingsType = contentSettingsType;
            mContentSettingValue = contentSettingValue;
        }

        public void run() {
            Website website =
                    createWebsiteWithContentSettingException(
                            mContentSettingsType, mContentSettingValue);
            mSettingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
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
            assertEquals(
                    "Switch check state is different than test setting.",
                    mContentSettingValue
                            == SingleWebsiteSettings.getEnabledValue(mContentSettingsType),
                    switchPref.isChecked());
        }
    }

    private static Website createWebsiteWithContentSettingException(
            @ContentSettingsType.EnumType int type, @ContentSettingValues int value) {
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        website.setContentSettingException(
                type,
                new ContentSettingException(
                        type,
                        website.getAddress().getOrigin(),
                        value,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));

        return website;
    }

    private static Website createWebsiteWithStorageAccessPermission(
            String origin, String embedder, @ContentSettingValues int setting) {
        Website website =
                new Website(WebsiteAddress.create(origin), WebsiteAddress.create(embedder));
        ContentSettingException info =
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        origin,
                        embedder,
                        ContentSettingValues.ASK,
                        ProviderType.NONE,
                        /* expiration= */ 0,
                        /* isEmbargoed= */ false);
        // Set setting explicitly to write it to prefs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    info.setContentSetting(ProfileManager.getLastUsedRegularProfile(), setting);
                });
        website.addEmbeddedPermission(info);
        return website;
    }
}
