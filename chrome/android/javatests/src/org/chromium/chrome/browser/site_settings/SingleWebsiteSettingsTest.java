// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.preference.Preference;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.ChosenObjectInfo;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.FileEditingInfo;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.media.MediaFeatures;
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

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    @Before
    public void setUp() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
    }

    /** A provider supplying params for {@link #testExceptionToggleShowing}. */
    public static class SingleWebsiteSettingsParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            ArrayList<ParameterSet> testCases = new ArrayList<>();
            for (@ContentSettingsType.EnumType
            int contentSettingsType : SiteSettingsUtil.SETTINGS_ORDER) {
                int enabled = SingleWebsiteSettings.getEnabledValue(contentSettingsType);
                testCases.add(createParameterSet("Enabled_", contentSettingsType, enabled));
                testCases.add(
                        createParameterSet("Block_", contentSettingsType, ContentSetting.BLOCK));
            }
            return testCases;
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(SingleWebsiteSettingsParams.class)
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testExceptionToggleShowing(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting int contentSettingValue) {
        // Preference should be added as a ChromeImageViewPreference. See
        // SingleWebsiteSettings#setUpNotificationsPreference
        Assume.assumeFalse(contentSettingsType == ContentSettingsType.NOTIFICATIONS);

        var approxGeoEnabled =
                PermissionsAndroidFeatureMap.isEnabled(
                        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);
        if (contentSettingsType == ContentSettingsType.GEOLOCATION && approxGeoEnabled) {
            return;
        }
        if (contentSettingsType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS
                && !approxGeoEnabled) {
            return;
        }

        new SingleExceptionTestCase(contentSettingsType, contentSettingValue).run();
    }

    @Test
    @SmallTest
    public void testNotificationException() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(
                        createWebsiteWithContentSettingException(
                                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    assertNotNull(
                            "Notification Preference not found.",
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.NOTIFICATIONS)));
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testGeolocationPermission() {
        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        GeolocationSetting blockSetting =
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK);
        runGeolocationTest(allowSetting, blockSetting, "Allowed • Precise", "Not allowed");
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testApproximateGeolocationPermission() {
        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK);
        GeolocationSetting blockSetting =
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK);
        runGeolocationTest(allowSetting, blockSetting, "Allowed • Approximate", "Not allowed");
    }

    private static void runGeolocationTest(
            GeolocationSetting allowSetting,
            GeolocationSetting blockSetting,
            String allowedText,
            String blockedText) {
        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.DURABLE);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals(allowedText, preference.getSummary());
        assertEquals(allowSetting, getGeolocationSetting(website));

        // Change to block.
        toggleLocationPermission();
        assertEquals(blockedText, preference.getSummary());
        assertEquals(blockSetting, getGeolocationSetting(website));

        // Change back to allow.
        toggleLocationPermission();
        assertEquals(allowedText, preference.getSummary());
        assertEquals(allowSetting, getGeolocationSetting(website));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testOneTimePreciseGeolocationPermission() {
        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        GeolocationSetting askSetting =
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.ONE_TIME);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed this time • Precise", preference.getSummary());
        assertEquals(allowSetting, getGeolocationSetting(website));

        // Delete one time permission.
        onView(withId(R.id.image_view_widget)).perform(click());
        assertNull(websitePreferences.findPreference(preferenceKey));
        assertEquals(askSetting, getGeolocationSetting(website));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testOneTimeApproximateGeolocationPermission() {
        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK);
        GeolocationSetting askSetting =
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.ONE_TIME);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed this time • Approximate", preference.getSummary());
        assertEquals(allowSetting, getGeolocationSetting(website));

        // Delete one time permission.
        onView(withId(R.id.image_view_widget)).perform(click());
        assertNull(websitePreferences.findPreference(preferenceKey));
        assertEquals(askSetting, getGeolocationSetting(website));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testGeolocationPermissionWithoutAppLevelPermission() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);

        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK);
        GeolocationSetting askSetting =
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.DURABLE);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed • Approximate", preference.getSummary());
        assertFalse(preference.isEnabled());

        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
        assertNotNull(warning);
        assertEquals(
                "To let Chromium access your location, also turn on location in Android Settings.",
                warning.getTitle().toString());

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testGeolocationPermissionWithOnlyCoarseAppLevelPermission() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);

        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        GeolocationSetting askSetting =
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.DURABLE);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed • Using approximate", preference.getSummary());
        assertTrue(preference.isEnabled());

        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
        assertNotNull(warning);
        assertEquals(
                "You can turn on precise location in Android Settings.",
                warning.getTitle().toString());

        // Open the location settings subpage.
        onView(withText(containsString("Location"))).perform(click());

        int summaryResId = R.string.website_settings_using_approximate_location_summary;
        // The subpage should show the summary on the 'Precise' option.
        onView(withText("Precise")).check(matches(hasSibling(withText(summaryResId))));

        // When 'Approximate' is selected, the summary should disappear.
        onView(withText(R.string.website_settings_permissions_geolocation_approximate))
                .perform(click());
        onView(withText(summaryResId)).check(doesNotExist());

        // When 'Precise' is selected again, the summary should reappear.
        onView(withText(R.string.website_settings_permissions_geolocation_precise))
                .perform(click());
        onView(withText("Precise")).check(matches(hasSibling(withText(summaryResId))));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @DisableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void
            testGeolocationPermissionWithOnlyCoarseAppLevelPermissionAndApproxGeolocationPermissionDisabled() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);

        Website website =
                createWebsiteWithGeolocationPermission(ContentSetting.ALLOW, SessionModel.DURABLE);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(ContentSettingsType.GEOLOCATION);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed", preference.getSummary());
        assertTrue(preference.isEnabled());
        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
        assertNull(warning);

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testGeolocationPermissionWithOnlyCoarseAppLevelPermissionOneTime() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);

        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.ONE_TIME);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed this time • Using approximate", preference.getSummary());
        assertTrue(preference.isEnabled());

        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
        assertNotNull(warning);
        assertEquals(
                "You can turn on precise location in Android Settings.",
                warning.getTitle().toString());

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @DisableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void
            testGeolocationPermissionWithOnlyCoarseAppLevelPermissionOneTimeAndApproxGeolocationPermissionDisabled() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);

        Website website =
                createWebsiteWithGeolocationPermission(ContentSetting.ALLOW, SessionModel.ONE_TIME);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(ContentSettingsType.GEOLOCATION);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed this time", preference.getSummary());
        assertTrue(preference.isEnabled());

        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
        assertNull(warning);

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testGeolocationPermissionWithSystemLocationDisabled() {
        // Disable android location permission.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ true);

        GeolocationSetting allowSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        GeolocationSetting askSetting =
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        Website website =
                createWebsiteWithGeolocationPermission(allowSetting, SessionModel.DURABLE);
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        var websitePreferences = (SingleWebsiteSettings) settingsActivity.getMainFragment();

        // Check initial state
        String preferenceKey =
                SingleWebsiteSettings.getPreferenceKey(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        Preference preference = websitePreferences.findPreference(preferenceKey);
        assertNotNull("Geolocation Preference not found.", preference);
        assertEquals("Allowed • Precise", preference.getSummary());
        assertFalse(preference.isEnabled());

        Preference warning =
                websitePreferences.findPreference(
                        SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING_EXTRA);
        assertNotNull(warning);
        assertEquals(
                "Location access is off for this device. Turn it on in Android Settings.",
                warning.getTitle().toString());

        settingsActivity.finish();
    }

    private static void toggleLocationPermission() {
        onView(
                        allOf(
                                withId(R.id.switch_container),
                                withParent(
                                        withParent(
                                                hasSibling(
                                                        withChild(
                                                                withText(
                                                                        containsString(
                                                                                "Location"))))))))
                .perform(click());
    }

    private static GeolocationSetting getGeolocationSetting(Website website) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return website.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS)
                            .getGeolocationSetting(ProfileManager.getLastUsedRegularProfile());
                });
    }

    @Test
    @SmallTest
    public void testDesktopSiteException() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(
                        createWebsiteWithContentSettingException(
                                ContentSettingsType.REQUEST_DESKTOP_SITE, ContentSetting.ALLOW));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    assertNotNull(
                            "Desktop site preference should be present.",
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.REQUEST_DESKTOP_SITE)));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
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
                 "serial-number": "3"}\
                """;
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
                        ContentSetting.ALLOW);
        Website website2 =
                createWebsiteWithStorageAccessPermission(
                        "https://[*.]embedded2.com",
                        "https://[*.]example.com",
                        ContentSetting.BLOCK);
        Website other =
                createWebsiteWithStorageAccessPermission(
                        "https://[*.]embedded.com", "https://[*.]foo.com", ContentSetting.BLOCK);
        Website merged =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        WebsiteAddress.create(EXAMPLE_ADDRESS), List.of(website, website2, other));

        var exceptions = merged.getEmbeddedPermissions().get(type);
        assertThat(exceptions.size()).isEqualTo(2);
        assertThat(exceptions.get(0).getContentSetting()).isEqualTo(ContentSetting.ALLOW);
        assertThat(exceptions.get(1).getContentSetting()).isEqualTo(ContentSetting.BLOCK);
        assertEquals(ContentSetting.BLOCK, getStorageAccessSetting(type, embedded2, example));

        // Open site settings.
        SettingsActivity activity = SiteSettingsTestUtils.startSingleWebsitePreferences(merged);
        onView(withText("embedded.com allowed")).check(matches(isDisplayed()));

        // Toggle the second permission.
        onView(withText("embedded2.com blocked")).check(matches(isDisplayed())).perform(click());
        assertEquals(ContentSetting.ALLOW, getStorageAccessSetting(type, embedded2, example));

        // Reset permission.
        onView(withText(containsString("reset"))).perform(click());
        onView(withText("Delete & reset")).perform(click());
        onView(withText("Embedded content")).check(doesNotExist());
        assertEquals(ContentSetting.ASK, getStorageAccessSetting(type, embedded2, example));
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
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return result[0];
    }

    /**
     * Helper function for creating a {@link ParameterSet} for {@link SingleWebsiteSettingsParams}.
     */
    private static ParameterSet createParameterSet(
            String namePrefix,
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting int contentSettingValue) {
        String prefKey = SingleWebsiteSettings.getPreferenceKey(contentSettingsType);
        assertNotNull(
                "Preference key is missing for ContentSettingsType <" + contentSettingsType + ">.",
                prefKey);

        return new ParameterSet()
                .name(namePrefix + prefKey)
                .value(contentSettingsType, contentSettingValue);
    }

    /** Test case class that check whether a toggle exists for a given content setting. */
    private static class SingleExceptionTestCase {
        @ContentSetting final int mContentSettingValue;
        @ContentSettingsType.EnumType final int mContentSettingsType;

        private SettingsActivity mSettingsActivity;

        SingleExceptionTestCase(
                @ContentSettingsType.EnumType int contentSettingsType,
                @ContentSetting int contentSettingValue) {
            mContentSettingsType = contentSettingsType;
            mContentSettingValue = contentSettingValue;
        }

        public void run() {
            Website website;
            if (mContentSettingsType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                website =
                        createWebsiteWithGeolocationPermission(
                                new GeolocationSetting(mContentSettingValue, mContentSettingValue),
                                SessionModel.DURABLE);
            } else {
                website =
                        createWebsiteWithContentSettingException(
                                mContentSettingsType, mContentSettingValue);
            }
            mSettingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SingleWebsiteSettings websitePreferences =
                                (SingleWebsiteSettings) mSettingsActivity.getMainFragment();
                        doTest(websitePreferences);
                    });

            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            mSettingsActivity.finish();
        }

        protected void doTest(SingleWebsiteSettings websitePreferences) {
            String prefKey = SingleWebsiteSettings.getPreferenceKey(mContentSettingsType);
            ChromeSwitchPreference switchPref = websitePreferences.findPreference(prefKey);
            assertNotNull("Preference cannot be found on screen.", switchPref);
            assertEquals(
                    "Switch check state is different than test setting.",
                    mContentSettingValue
                            == SingleWebsiteSettings.getEnabledValue(mContentSettingsType),
                    switchPref.isChecked());
        }
    }

    private static Website createWebsiteWithContentSettingException(
            @ContentSettingsType.EnumType int type, @ContentSetting int value) {
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

    private static Website createWebsiteWithGeolocationPermission(
            GeolocationSetting setting, int sessionModel) {
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        website.getAddress().getOrigin(),
                        website.getAddress().getOrigin(),
                        /* isEmbargoed= */ false,
                        sessionModel);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        info.setGeolocationSetting(
                                ProfileManager.getLastUsedRegularProfile(), setting));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        website.setPermissionInfo(info);
        return website;
    }

    private static Website createWebsiteWithGeolocationPermission(
            @ContentSetting int setting, int sessionModel) {
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        website.getAddress().getOrigin(),
                        website.getAddress().getOrigin(),
                        /* isEmbargoed= */ false,
                        sessionModel);
        ThreadUtils.runOnUiThreadBlocking(
                () -> info.setContentSetting(ProfileManager.getLastUsedRegularProfile(), setting));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        website.setPermissionInfo(info);
        return website;
    }

    private static Website createWebsiteWithStorageAccessPermission(
            String origin, String embedder, @ContentSetting int setting) {
        Website website =
                new Website(WebsiteAddress.create(origin), WebsiteAddress.create(embedder));
        ContentSettingException info =
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        origin,
                        embedder,
                        ContentSetting.ASK,
                        ProviderType.NONE,
                        /* expirationInDays= */ 0,
                        /* isEmbargoed= */ false);
        // Set setting explicitly to write it to prefs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    info.setContentSetting(ProfileManager.getLastUsedRegularProfile(), setting);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        website.addEmbeddedPermission(info);
        return website;
    }
}
