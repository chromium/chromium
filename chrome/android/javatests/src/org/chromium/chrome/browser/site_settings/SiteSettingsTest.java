// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;

import static org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED;
import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForViewCheckingState;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.FederatedIdentityTestUtils;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.BinaryStatePermissionPreference;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.RwsCookieInfo;
import org.chromium.components.browser_ui.site_settings.RwsCookieSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.TriStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.TriStateSiteSettingsPreference;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.media.MediaFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for everything under Settings > Site Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
// TODO(https://crbug.com/464016211): these tests could be flaky because of AnimatedProgressBar.
@DisableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
// TODO(crbug.com/344672098): Failing when batched, batch this again.
public class SiteSettingsTest {
    private static final int RENDER_TEST_REVISION = 6;
    @ClassRule public static PermissionTestRule mPermissionRule = new PermissionTestRule(true);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mPermissionRule, false);

    public AdvancedProtectionTestRule mAdvancedProtectionRule = new AdvancedProtectionTestRule();

    // {@link AdvancedProtectionTestRule} needs to run prior to profile being created.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAdvancedProtectionRule).around(mBlankCTATabInitialStateRule);

    @Mock private SettingsNavigation mSettingsNavigation;

    private PermissionUpdateWaiter mPermissionUpdateWaiter;

    private static final String[] NULL_ARRAY = new String[0];
    private static final String[] BINARY_TOGGLE_AND_INFO_TEXT =
            new String[] {"info_text", "binary_toggle"};
    private static final String[] BINARY_TOGGLE = new String[] {"binary_toggle"};
    private static final String[] BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT =
            new String[] {"info_text", "binary_toggle", "add_exception"};
    private static final String[] BINARY_TOGGLE_WITH_EXCEPTION =
            new String[] {"binary_toggle", "add_exception"};
    private static final String[] BINARY_TOGGLE_WITH_OS_WARNING =
            new String[] {"binary_toggle", "os_permissions_warning"};
    private static final String[] BINARY_TOGGLE_WITH_OS_WARNING_EXTRA =
            new String[] {"binary_toggle", "os_permissions_warning_extra"};
    private static final String[] BINARY_TOGGLE_WITH_OS_WARNING_AND_OS_WARNING_EXTRA =
            new String[] {
                "binary_toggle", "os_permissions_warning", "os_permissions_warning_extra"
            };
    private static final String[] BINARY_RADIO_BUTTON_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button"};
    private static final String[] BINARY_RADIO_BUTTON = new String[] {"binary_radio_button"};
    private static final String[] BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "add_exception"};
    private static final String[] BINARY_RADIO_BUTTON_WITH_EXCEPTION =
            new String[] {"binary_radio_button", "add_exception"};
    private static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA =
            new String[] {"binary_radio_button", "os_permissions_warning_extra"};
    private static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "os_permissions_warning"};
    private static final String[] BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT =
            new String[] {"info_text", "binary_radio_button", "os_permissions_warning_extra"};
    private static final String[]
            BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_OS_WARNING_EXTRA_AND_INFO_TEXT =
                    new String[] {
                        "info_text",
                        "binary_radio_button",
                        "os_permissions_warning",
                        "os_permissions_warning_extra"
                    };
    private static final String[] CLEAR_BROWSING_DATA_LINK =
            new String[] {"clear_browsing_data_link", "clear_browsing_divider"};

    private static final String[] CLEAR_BROWSING_DATA_LINK_WITH_CONTAINMENT =
            new String[] {"clear_browsing_data_link"};
    private static final String[] ANTI_ABUSE_PREF_KEYS = {
        "anti_abuse_when_on_header",
        "anti_abuse_when_on_section_one",
        "anti_abuse_when_on_section_two",
        "anti_abuse_when_on_section_three",
        "anti_abuse_things_to_consider_header",
        "anti_abuse_things_to_consider_section_one"
    };
    private static final String[] BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS = {
        "binary_toggle",
        "anti_abuse_when_on_header",
        "anti_abuse_when_on_section_one",
        "anti_abuse_when_on_section_two",
        "anti_abuse_when_on_section_three",
        "anti_abuse_things_to_consider_header",
        "anti_abuse_things_to_consider_section_one"
    };

    private static final String PRIMARY_PATTERN_WITH_WILDCARD = "http://[*.]primary.com";
    private static final String SECONDARY_PATTERN_WITH_WILDCARD = "http://[*.]secondary.com";

    @Before
    public void setUp() throws TimeoutException {
        // Clean up cookies and permissions to ensure tests run in a clean environment.
        cleanUpCookiesAndPermissions();
    }

    @After
    public void tearDown() throws TimeoutException {
        if (mPermissionUpdateWaiter != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mPermissionRule
                                .getActivityTab()
                                .removeObserver(mPermissionUpdateWaiter);
                    });
        }

        CallbackHelper helper = new CallbackHelper();
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

        LocationUtils.setFactory(null);
        LocationProviderOverrider.setLocationProviderImpl(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY)
                .apply();
    }

    @AfterClass
    public static void tearDownAfterClass() throws TimeoutException {
        cleanUpCookiesAndPermissions();
    }

    private static BrowserContextHandle getBrowserContextHandle() {
        return ProfileManager.getLastUsedRegularProfile();
    }

    private void initializeUpdateWaiter(final boolean expectGranted) {
        if (mPermissionUpdateWaiter != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mPermissionRule
                                .getActivityTab()
                                .removeObserver(mPermissionUpdateWaiter);
                    });
        }
        Tab tab = mPermissionRule.getActivityTab();

        mPermissionUpdateWaiter =
                new PermissionUpdateWaiter(
                        expectGranted ? "Granted" : "Denied", mPermissionRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(mPermissionUpdateWaiter));
    }

    private void triggerEmbargoForOrigin(String url) throws TimeoutException {
        // Ignore notification request 4 times to enter embargo. 5th one ensures that notifications
        // are blocked by actually causing a deny-by-embargo.
        for (int i = 0; i < 5; i++) {
            mPermissionRule.loadUrl(url);
            mPermissionRule.runJavaScriptCodeInCurrentTab("requestPermissionAndRespond()");
        }
    }

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    private static void cleanUpCookiesAndPermissions() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {
                                        BrowsingDataType.SITE_DATA, BrowsingDataType.SITE_SETTINGS
                                    },
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    /**
     * Returns a {@link Matcher} for a preference's managed disclaimer, depending on highlighting of
     * managed prefs being enabled. Use {@code activeView} as true for the view that's is supposed
     * to be shown when the preference is managed, or as false for the view that is always supposed
     * to be hidden because of the highlighting experiment.
     */
    private static Matcher<View> getManagedViewMatcher(boolean activeView) {
        return activeView
                ? allOf(
                        withId(R.id.managed_disclaimer_text),
                        hasSibling(withId(R.id.radio_button_layout)))
                : withId(R.id.managed_view_legacy);
    }

    private void createCookieExceptions() {
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

    private void createCookieExceptionsWithWildcards() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "*",
                            SECONDARY_PATTERN_WITH_WILDCARD,
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            PRIMARY_PATTERN_WITH_WILDCARD,
                            "*",
                            ContentSetting.ALLOW);
                });
    }

    private void createStorageAccessExceptions() {
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

    private Website getStorageAccessSite() {
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

    private void createAndSetRwsCookieInfo(Website owner, List<Website> websiteList) {
        RwsCookieInfo rwsInfo = new RwsCookieInfo(owner.getAddress().getOrigin(), websiteList);
        owner.setRwsCookieInfo(rwsInfo);
    }

    private Website getRwsOwnerSite() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        createAndSetRwsCookieInfo(origin1, List.of(origin1, origin2));
        return origin1;
    }

    private WebsiteGroup getRwsSiteGroup() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        createAndSetRwsCookieInfo(origin1, List.of(origin1, origin2));
        return new WebsiteGroup(origin1.getAddress().getOrigin(), Arrays.asList(origin1, origin2));
    }

    /** Sets Allow Location Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures({
        ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    public void testSetAllowLocationEnabledWithToggle() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCaseWithToggle(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        true)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "Location should be allowed.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        initializeUpdateWaiter(/* expectGranted= */ true);

        // Launch a page that uses geolocation and make sure a permission prompt shows up.
        mPermissionRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html",
                "",
                0,
                false,
                true);
    }

    /** Sets Allow Location Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testSetAllowLocationEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "Location should be allowed.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        initializeUpdateWaiter(/* expectGranted= */ true);

        // Launch a page that uses geolocation and make sure a permission prompt shows up.
        mPermissionRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html",
                "",
                0,
                false,
                true);
    }

    /** Sets Allow Location Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testSetAllowLocationNotEnabledWithToggle() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCaseWithToggle(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        false)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Location should be blocked.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        // Launch a page that uses geolocation. No permission prompt is expected.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html",
                "",
                0,
                false,
                true);
    }

    /** Sets Allow Location Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testSetAllowLocationNotEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Location should be blocked.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        // Launch a page that uses geolocation. No permission prompt is expected.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html",
                "",
                0,
                false,
                true);
    }

    private void setCookiesEnabled(final SettingsActivity settingsActivity, final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        final SingleCategorySettings websitePreferences =
                                (SingleCategorySettings) settingsActivity.getMainFragment();
                        final TriStateCookieSettingsPreference cookies =
                                websitePreferences.findPreference(
                                        SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);

                        websitePreferences.onPreferenceChange(
                                cookies,
                                enabled
                                        ? CookieControlsMode.INCOGNITO_ONLY
                                        : CookieControlsMode.BLOCK_THIRD_PARTY);
                        Assert.assertEquals(
                                "Cookies should be " + (enabled ? "allowed" : "blocked"),
                                doesAcceptCookies(),
                                enabled);
                    }

                    private boolean doesAcceptCookies() {
                        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getInteger(COOKIE_CONTROLS_MODE)
                                == CookieControlsMode.INCOGNITO_ONLY;
                    }
                });
    }

    private enum ToggleButtonState {
        EnabledUnchecked,
        EnabledChecked,
        Disabled
    }

    /** Checks if the button representing the given state matches the managed expectation. */
    private void checkTriStateCookieToggleButtonState(
            final SettingsActivity settingsActivity,
            final @CookieControlsMode int state,
            final ToggleButtonState toggleState) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    TriStateCookieSettingsPreference triStateCookieToggle =
                            preferences.findPreference(
                                    SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);
                    boolean enabled = toggleState != ToggleButtonState.Disabled;
                    boolean checked = toggleState == ToggleButtonState.EnabledChecked;
                    Assert.assertEquals(
                            state + " button should be " + (enabled ? "enabled" : "disabled"),
                            enabled,
                            triStateCookieToggle.isButtonEnabledForTesting(state));
                    Assert.assertEquals(
                            state + " button should be " + (checked ? "checked" : "unchecked"),
                            checked,
                            triStateCookieToggle.isButtonCheckedForTesting(state));
                });
    }

    private void checkDefaultCookiesSettingManaged(boolean expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Default Cookie Setting should be "
                                    + (expected ? "managed" : "unmanaged"),
                            expected,
                            WebsitePreferenceBridge.isContentSettingManaged(
                                    getBrowserContextHandle(), ContentSettingsType.COOKIES));
                });
    }

    private void checkThirdPartyCookieBlockingManaged(boolean expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Third Party Cookie Blocking should be "
                                    + (expected ? "managed" : "unmanaged"),
                            expected,
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .isManagedPreference(COOKIE_CONTROLS_MODE));
                });
    }

    private void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
                        TriStateCookieSettingsPreference preference =
                                preferences.findPreference(
                                        SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);
                        preferences.onPreferenceChange(
                                preference,
                                enabled
                                        ? CookieControlsMode.INCOGNITO_ONLY
                                        : CookieControlsMode.BLOCK_THIRD_PARTY);
                    } else if (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
                            && type != SiteSettingsCategory.Type.ANTI_ABUSE) {
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
            onViewWaiting(
                            withText(id),
                            // checkRootDialog=true ensures dialog is in focus, avoids flakiness.
                            true)
                    .perform(click());
        }
        settingsActivity.finish();
    }

    private void setGlobalTriStateToggleForCategory(
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
    private void checkPreferencesForCategory(
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

    private void checkPreferencesForSettingsActivity(
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

    private void testExpectedPreferences(
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

    /** Allows cookies to be set and ensures that they are. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesNotBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        setCookiesEnabled(settingsActivity, true);
        settingsActivity.finish();

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie still is set.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /** Clicks on cookies radio buttons and verify the right FPS subpage is launched. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesFpsSubpageIsLaunched() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        verifyFpsCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY);
        verifyFpsCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieControlsMode.INCOGNITO_ONLY);
    }

    private void verifyFpsCookieSubpageIsLaunchedWithParams(
            final SettingsActivity settingsActivity,
            @CookieControlsMode int expectedCookieControlMode) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleCategorySettings websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    final TriStateCookieSettingsPreference cookies =
                            websitePreferences.findPreference(
                                    SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);

                    Mockito.clearInvocations(mSettingsNavigation);
                    websitePreferences.setSettingsNavigation(mSettingsNavigation);

                    SiteSettingsTestUtils.getCookieRadioButtonFrom(
                                    cookies, expectedCookieControlMode)
                            .getAuxButtonForTests()
                            .performClick();

                    Bundle fragmentArgs = new Bundle();
                    fragmentArgs.putInt(
                            RwsCookieSettings.EXTRA_COOKIE_PAGE_STATE, expectedCookieControlMode);

                    Mockito.verify(mSettingsNavigation)
                            .startSettings(
                                    eq(websitePreferences.getContext()),
                                    eq(RwsCookieSettings.class),
                                    refEq(fragmentArgs),
                                    eq(true));
                });
    }

    /** Blocks specific sites from setting cookies and ensures that no cookies can be set. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1395173")
    public void testSiteExceptionSiteDataBlocked() throws Exception {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.SITE_DATA, true);

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Check cookies can be set for this website when there is no rule.
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Set specific rule to block site and ensure it cannot set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");

        setGlobalToggleForCategory(SiteSettingsCategory.Type.SITE_DATA, false);
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /** Set a cookie and check that it is removed when a site is cleared. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1112409")
    public void testClearCookies() throws Exception {
        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);

        resetSite(WebsiteAddress.create(url));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }

    /** Tests clearing cookies for a group of websites. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testClearCookiesGroup() throws Exception {
        final String url1 =
                mPermissionRule.getURLWithHostName(
                        "one.example.com", "/chrome/test/data/android/cookie.html");
        final String url2 =
                mPermissionRule.getURLWithHostName(
                        "two.example.com", "/chrome/test/data/android/cookie.html");
        final String url3 =
                mPermissionRule.getURLWithHostName(
                        "foo.com", "/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url1);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".one.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        mPermissionRule.loadUrl(url2);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".two.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        mPermissionRule.loadUrl(url3);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".foo.com\")");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);

        resetGroup(Arrays.asList(WebsiteAddress.create(url1), WebsiteAddress.create(url2)));

        // 1 and 2 got cleared; 3 stays intact.
        mPermissionRule.loadUrl(url1);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.loadUrl(url2);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.loadUrl(url3);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }

    /** Set cookies for domains and check that they are removed when a site is cleared. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1329450")
    public void testClearDomainCookies() throws Exception {
        final String url =
                mPermissionRule.getURLWithHostName(
                        "test.example.com", "/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".test.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create("test.example.com"));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set the cookie content setting to allow through policy and ensure the correct radio buttons
     * are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({@Policies.Item(key = "DefaultCookiesSetting", string = "1")})
    public void testDefaultCookiesSettingManagedAllow() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to ALLOW) while ThirdPartyCookieBlocking is not
        // managed. This means that every button other than BLOCK is enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.EnabledChecked);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.EnabledUnchecked);
        // TODO(crbug.com/40064993): fix this assertion.
        // onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Enable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({@Policies.Item(key = "BlockThirdPartyCookies", string = "true")})
    public void testBlockThirdPartyCookiesManagedTrue() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to true) while the ContentSetting is not
        // managed. This means a user can choose only between BLOCK_THIRD_PARTY and BLOCK, so only
        // these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity, CookieControlsMode.INCOGNITO_ONLY, ToggleButtonState.Disabled);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.EnabledChecked);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));

        SingleCategorySettings singleCategorySettings =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        Preference addExceptionPreference =
                singleCategorySettings.findPreference(SingleCategorySettings.ADD_EXCEPTION_KEY);
        Assert.assertTrue(addExceptionPreference.isEnabled());

        settingsActivity.finish();
    }

    /**
     * Disable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({@Policies.Item(key = "BlockThirdPartyCookies", string = "false")})
    public void testBlockThirdPartyCookiesManagedFalse() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to false) while the ContentSetting is not
        // managed. This means a user can only choose to ALLOW all cookies or BLOCK all cookies, so
        // only these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.EnabledChecked);
        checkTriStateCookieToggleButtonState(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY, ToggleButtonState.Disabled);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Set both the cookie content setting and third-party cookie blocking through policy and ensure
     * the correct radio buttons are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultCookiesSetting", string = "1"),
        @Policies.Item(key = "BlockThirdPartyCookies", string = "false")
    })
    public void testAllCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(true);
        // The ContentSetting and ThirdPartyCookieBlocking are managed. This means a user has a
        // fixed setting for cookies that they cannot change. Therefore, all buttons except the
        // selected one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.EnabledChecked);
        checkTriStateCookieToggleButtonState(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY, ToggleButtonState.Disabled);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /** Ensure no radio buttons are enforced when cookie settings are unmanaged. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testNoCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting and ThirdPartyCookieBlocking are unmanaged. This means all buttons
        // should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.EnabledChecked);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /** Ensure correct radio buttons are shown. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void blockAndAllowThirdPartyCookieOptionsShown() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.EnabledChecked);
        checkTriStateCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    private void resetSite(WebsiteAddress address) {
        Website website = new Website(address, address);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    websitePreferences.resetSite();
                });
        settingsActivity.finish();
    }

    private void resetGroup(List<WebsiteAddress> addresses) {
        List<Website> sites = new ArrayList<>();
        for (WebsiteAddress address : addresses) {
            Website website = new Website(address, address);
            sites.add(website);
        }
        WebsiteGroup group = new WebsiteGroup(addresses.get(0).getDomainAndRegistry(), sites);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startGroupedWebsitesPreferences(group);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GroupedWebsitesSettings websitePreferences =
                            (GroupedWebsitesSettings) settingsActivity.getMainFragment();
                    websitePreferences.resetGroup();
                });
        settingsActivity.finish();
    }

    /** Sets Allow Popups Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testPopupsBlockedWithToggle() throws TimeoutException {
        new TwoStatePermissionTestCaseWithToggle(
                        "Popups",
                        SiteSettingsCategory.Type.POPUPS,
                        ContentSettingsType.POPUPS,
                        false)
                .run();

        // Test that the popup doesn't open.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(1, getTabCount());
    }

    /** Sets Allow Popups Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testPopupsBlocked() throws TimeoutException {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Popups",
                        SiteSettingsCategory.Type.POPUPS,
                        ContentSettingsType.POPUPS,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the popup doesn't open.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(1, getTabCount());
    }

    /** Sets Allow Popups Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testPopupsNotBlocked() throws TimeoutException {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Popups",
                        SiteSettingsCategory.Type.POPUPS,
                        ContentSettingsType.POPUPS,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that a popup opens.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(2, getTabCount());
    }

    /** Test that showing the Site Settings menu doesn't crash (crbug.com/610576). */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenu() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        settingsActivity.finish();
    }

    /**
     * Test that showing the Site Settings menu contains the "Third-party cookies" and "Site data"
     * rows.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenuWithPrivacySandboxSettings4Enabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNull(websitePreferences.findPreference("cookies"));
        assertNotNull(websitePreferences.findPreference("third_party_cookies"));
        assertNotNull(websitePreferences.findPreference("site_data"));
        settingsActivity.finish();
    }

    /** Test that showing the Site Settings menu contains the "Tracking protection" row. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testSiteSettingsMenuWithTrackingProtectionEnabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        final Preference trackingProtectionPreference =
                websitePreferences.findPreference("tracking_protection");
        assertNotNull(trackingProtectionPreference);
        Assert.assertEquals(
                trackingProtectionPreference.getTitle(),
                websitePreferences
                        .getContext()
                        .getString(R.string.third_party_cookies_link_row_label));
        settingsActivity.finish();
    }

    /** Test that showing the Site Settings menu contains the "Anti-abuse" row. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenuForAntiAbuse() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNotNull(websitePreferences.findPreference("anti_abuse"));
        settingsActivity.finish();
    }

    /**
     * Tests that only expected Preferences are shown for a category. This santiy checks the number
     * of categories only. Each category has its own individual test below.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesShown() {
        // If you add a category in the SiteSettings UI, please update this total AND add a test for
        // it below, named "testOnlyExpectedPreferences<Category>".
        Assert.assertEquals(38, SiteSettingsCategory.Type.NUM_ENTRIES);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesZoom() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.ZOOM, NULL_ARRAY);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testOnlyExpectedPreferencesAllSites() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.ALL_SITES, CLEAR_BROWSING_DATA_LINK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testOnlyExpectedPreferencesAllSites_ContainmentEnabled() {
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.ALL_SITES, CLEAR_BROWSING_DATA_LINK_WITH_CONTAINMENT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAdsWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.ADS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAds() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.ADS,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAntiAbuse() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.ANTI_ABUSE,
                BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS,
                BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAugmentedRealityWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUGMENTED_REALITY, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAugmentedReality() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUGMENTED_REALITY,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAutoDarkWebContentWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                BINARY_TOGGLE,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAutoDarkWebContent() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                BINARY_RADIO_BUTTON,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAutoPictureInPictureWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.WINDOW_MANAGEMENT, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    public void testOnlyExpectedPreferencesAutoPictureInPicture() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAutomaticDownloadsWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesAutomaticDownloads() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBackgroundSyncWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BACKGROUND_SYNC,
                BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBackgroundSync() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BACKGROUND_SYNC,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBluetoothWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.BLUETOOTH, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBluetooth() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH, BINARY_RADIO_BUTTON, BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBluetoothScanningWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesBluetoothScanning() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                BINARY_RADIO_BUTTON,
                BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesCameraWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.CAMERA, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesCamera() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.CAMERA,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesClipboardWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.CLIPBOARD, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesClipboard() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.CLIPBOARD,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesFileEditingWithToggle() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.FILE_EDITING, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesFileEditing() {
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.FILE_EDITING, BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesThirdPartyCookies() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                new String[] {"info_text", "tri_state_cookie_toggle", "add_exception"},
                new String[] {"info_text", "tri_state_cookie_toggle", "add_exception"});
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSiteDataWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SITE_DATA,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSiteData() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SITE_DATA,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedExceptionsSiteData() {
        createCookieExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.SITE_DATA);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("secondary.com")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedExceptionsThirdPartyCookies() {
        createCookieExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        onView(withText("primary.com")).check(doesNotExist());
        onView(withText("secondary.com")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void shouldShowWildcardsInExceptionsOnThirdPartyCookiesPage() {
        createCookieExceptionsWithWildcards();
        SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        onView(withText(PRIMARY_PATTERN_WITH_WILDCARD)).check(doesNotExist());
        onView(withText(SECONDARY_PATTERN_WITH_WILDCARD)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void shouldShowWildcardsInExceptionsOnSiteDataPage() {
        createCookieExceptionsWithWildcards();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.SITE_DATA);

        onView(withText(PRIMARY_PATTERN_WITH_WILDCARD)).check(matches(isDisplayed()));
        onView(withText(SECONDARY_PATTERN_WITH_WILDCARD)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesStorageAccessWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.STORAGE_ACCESS,
                BINARY_TOGGLE_AND_INFO_TEXT,
                BINARY_TOGGLE_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesStorageAccess() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.STORAGE_ACCESS,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    // TODO(crbug.com/433576895): Re-enable containment feature once the test is fixed.
    @DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testExpectedExceptionsStorageAccess() {
        createStorageAccessExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.STORAGE_ACCESS);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("2 sites")).check(matches(isDisplayed()));
        onView(withText("primary2.com")).check(matches(isDisplayed()));
        onView(withText("1 site")).check(matches(isDisplayed()));

        getImageViewWidget("primary.com").check(matches(isDisplayed())).perform(click());

        // Check that the subpage is shown with the correct origins.
        onView(withText("primary.com")).check(matches(isDisplayed()));
        onViewWaiting(withText("secondary.com")).check(matches(isDisplayed()));
        onViewWaiting(withText("secondary3.com")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    // TODO(crbug.com/433576895): Re-enable containment feature once the test is fixed.
    @DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testResetExceptionGroupStorageAccess() {
        createStorageAccessExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.STORAGE_ACCESS);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("2 sites")).check(matches(isDisplayed()));
        onView(withText("primary2.com")).check(matches(isDisplayed()));
        onView(withText("1 site")).check(matches(isDisplayed()));

        onView(withText("primary.com")).perform(click());
        onView(withText("Remove")).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary.com")));
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary2.com"),
                                    new GURL("https://secondary2.com")));
                });

        onView(withText("primary.com")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    // TODO(crbug.com/433576895): Re-enable containment feature once the test is fixed.
    @DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testBlockExceptionGroupStorageAccess() {
        createStorageAccessExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.STORAGE_ACCESS);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("2 sites")).check(matches(isDisplayed()));
        onView(withText("primary2.com")).check(matches(isDisplayed()));
        onView(withText("1 site")).check(matches(isDisplayed()));

        onView(withText("primary.com")).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Block")).perform(click());
        onView(withText("Confirm")).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.BLOCK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary.com")));
                    assertEquals(
                            ContentSetting.BLOCK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary2.com"),
                                    new GURL("https://secondary2.com")));
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testStorageAccessSubpage() {
        createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startStorageAccessSettingsActivity(getStorageAccessSite());

        onViewWaiting(withText("secondary1.com")).check(matches(isDisplayed()));
        onViewWaiting(withText("secondary3.com")).check(matches(isDisplayed()));

        // Reset first permission.
        getImageViewWidget("secondary1.com").check(matches(isDisplayed())).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary1.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                });

        waitForViewCheckingState(
                withText("secondary1.com"), VIEW_INVISIBLE | VIEW_NULL | VIEW_GONE);
        onView(withText("secondary1.com")).check(doesNotExist());
        onView(withText("secondary3.com")).check(matches(isDisplayed()));

        // Reset second permission.
        getImageViewWidget("secondary3.com").check(matches(isDisplayed())).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                });

        // Check that, because there aren't any permissions to show, the activity is closed.
        Assert.assertTrue(settingsActivity.isFinishing());
    }

    private ViewInteraction getImageViewWidget(String preferenceTitle) {
        return onView(
                allOf(
                        withId(R.id.image_view_widget),
                        isDescendantOfA(withChild(withChild(withText(preferenceTitle))))));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExpectedCookieButtonsCheckedWhenFpsUiEnabled() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    TriStateCookieSettingsPreference cookieToggle =
                            preferences.findPreference(
                                    SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);

                    clickButtonAndVerifyItsChecked(cookieToggle, CookieControlsMode.INCOGNITO_ONLY);
                    clickButtonAndVerifyItsChecked(
                            cookieToggle, CookieControlsMode.BLOCK_THIRD_PARTY);
                });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExpectedCookieButtonsCheckedWhenFpsUiAndPrivacySandboxSettings4Enabled() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    TriStateCookieSettingsPreference threeStateCookieToggle =
                            preferences.findPreference(
                                    SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);

                    clickButtonAndVerifyItsChecked(
                            threeStateCookieToggle, CookieControlsMode.INCOGNITO_ONLY);
                    clickButtonAndVerifyItsChecked(
                            threeStateCookieToggle, CookieControlsMode.BLOCK_THIRD_PARTY);
                });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAllThirdPartyCookiesSnackbarDisplayedWhenTopicsEnabled() {
        var userActionTester = new UserActionTester();
        // Enable Topics API.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService = UserPrefs.get(getBrowserContextHandle());
                    prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, true);
                });
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        // Select the block all 3PC option.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickButtonAndVerifyItsChecked(
                            getTriStateToggle(settingsActivity),
                            CookieControlsMode.BLOCK_THIRD_PARTY);
                });
        // The snackbar should be displayed.
        onView(withText(R.string.privacy_sandbox_snackbar_message)).check(matches(isDisplayed()));
        Assert.assertTrue(
                "User action is not recorded",
                userActionTester.getActions().contains("Settings.PrivacySandbox.Block3PCookies"));
        // Click a different button, check that the snackbar was dismissed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickButtonAndVerifyItsChecked(
                            getTriStateToggle(settingsActivity), CookieControlsMode.INCOGNITO_ONLY);
                });
        onView(withText(R.string.privacy_sandbox_snackbar_message)).check(doesNotExist());
        // Click back, click on the more button to test that the settings fragment was open.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clickButtonAndVerifyItsChecked(
                            getTriStateToggle(settingsActivity),
                            CookieControlsMode.BLOCK_THIRD_PARTY);
                });
        onView(withText(R.string.privacy_sandbox_snackbar_message)).check(matches(isDisplayed()));
        onView(withText(R.string.more)).perform(click());
        onViewWaiting(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    private TriStateCookieSettingsPreference getTriStateToggle(SettingsActivity settingsActivity) {
        SingleCategorySettings preferences =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        return preferences.findPreference(SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);
    }

    private void clickButtonAndVerifyItsChecked(
            TriStateCookieSettingsPreference threeStateCookieToggle,
            @CookieControlsMode int state) {
        threeStateCookieToggle.getButton(state).performClick();
        Assert.assertTrue(
                "Button should be checked.", threeStateCookieToggle.getButton(state).isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures({
        ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testOnlyExpectedPreferencesDeviceLocationWithToggle() {
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ true);
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        testExpectedPreferences(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE, BINARY_TOGGLE);

        // Disable system location setting and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ true);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE_WITH_OS_WARNING_EXTRA);

        // Disable android location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE_WITH_OS_WARNING);

        // Disable android fine location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE_WITH_OS_WARNING);

        // Disable system location setting and android location permission and check for the right
        // preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_TOGGLE_WITH_OS_WARNING_AND_OS_WARNING_EXTRA);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION
    })
    public void testOnlyExpectedPreferencesDeviceLocation() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        testExpectedPreferences(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);

        // Disable system location setting and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ true);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);

        // Disable android location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT);

        // Disable android fine location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT);

        // Disable system location setting and android location permission and check for the right
        // preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesFederatedIdentityApiWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesFederatedIdentityApi() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesJavascriptOptimizerWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesJavascriptOptimizer() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesHandTrackingWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.HAND_TRACKING, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesHandTracking() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.HAND_TRACKING, BINARY_RADIO_BUTTON, BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesIdleDetectionWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.IDLE_DETECTION, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesIdleDetection() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.IDLE_DETECTION,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesLocalNetworkAccessWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.LOCAL_NETWORK_ACCESS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesLocalNetworkAccess() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.LOCAL_NETWORK_ACCESS,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesWindowManagementWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.WINDOW_MANAGEMENT, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesWindowManagement() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesJavascriptWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT,
                BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesJavascript() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesMicrophoneWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.MICROPHONE, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesMicrophone() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.MICROPHONE,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesNfcWithToggle() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);

        testExpectedPreferences(SiteSettingsCategory.Type.NFC, BINARY_TOGGLE, BINARY_TOGGLE);

        // Disable system nfc setting and check for the right preferences.
        NfcSystemLevelSetting.setNfcSettingForTesting(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC, BINARY_TOGGLE_WITH_OS_WARNING_EXTRA);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);

        testExpectedPreferences(
                SiteSettingsCategory.Type.NFC,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);

        // Disable system nfc setting and check for the right preferences.
        NfcSystemLevelSetting.setNfcSettingForTesting(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("QuietNotificationPrompts")
    @DisableFeatures({
        ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    public void testOnlyExpectedPreferencesNotificationsWithToggle() {
        String[] notificationsEnabled = new String[] {"binary_toggle", "notifications_quiet_ui"};
        String[] notificationsDisabled = BINARY_TOGGLE;

        testExpectedPreferences(
                SiteSettingsCategory.Type.NOTIFICATIONS,
                notificationsDisabled,
                notificationsEnabled);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        "QuietNotificationPrompts",
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    public void testOnlyExpectedPreferencesNotifications() {
        String[] notificationsEnabled =
                new String[] {"info_text", "binary_radio_button", "notifications_quiet_ui"};
        String[] notificationsDisabled = BINARY_RADIO_BUTTON_AND_INFO_TEXT;

        testExpectedPreferences(
                SiteSettingsCategory.Type.NOTIFICATIONS,
                notificationsDisabled,
                notificationsEnabled);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesPopupsWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.POPUPS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesPopups() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.POPUPS,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesProtectedMediaWithToggle() {
        String[] protectedMedia = new String[] {"tri_state_toggle", "protected_content_learn_more"};
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ALLOW);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesProtectedMedia() {
        String[] protectedMedia = new String[] {"info_text", "tri_state_toggle"};
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ALLOW);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesRequestDesktopSiteWithToggle() {
        String[] rdsEnabled = {"binary_toggle", "desktop_site_window", "add_exception"};
        testExpectedPreferences(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                BINARY_TOGGLE_WITH_EXCEPTION,
                rdsEnabled);
        Assert.assertTrue(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be"
                        + " updated.",
                ContextUtils.getAppSharedPreferences()
                        .contains(
                                SingleCategorySettingsConstants
                                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesRequestDesktopSite() {
        String[] rdsEnabled = {
            "info_text", "binary_radio_button", "desktop_site_window", "add_exception"
        };
        testExpectedPreferences(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                rdsEnabled);
        Assert.assertTrue(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be"
                        + " updated.",
                ContextUtils.getAppSharedPreferences()
                        .contains(
                                SingleCategorySettingsConstants
                                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSensorsWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.SENSORS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSensors() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SENSORS,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSoundWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SOUND,
                BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSound() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SOUND,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesUsbWithToggle() {
        testExpectedPreferences(SiteSettingsCategory.Type.USB, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesUsb() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.USB,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSerialPortWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SERIAL_PORT, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesSerialPort() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.SERIAL_PORT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesUseStorage() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.USE_STORAGE, NULL_ARRAY);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesVirtualRealityWithToggle() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.VIRTUAL_REALITY, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOnlyExpectedPreferencesVirtualReality() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.VIRTUAL_REALITY,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    /** Tests system NFC support in Preferences. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testSystemNfcSupport() {
        // Disable system nfc support and check for the right preferences.
        NfcSystemLevelSetting.setNfcSupportForTesting(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    /**
     * Tests that {@link SingleWebsiteSettings#resetSite} doesn't crash (see e.g. the crash on host
     * names in issue 600232).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDoesntCrash() {
        WebsiteAddress address = WebsiteAddress.create("example.com");
        resetSite(address);
    }

    /** Sets Allow Camera Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // crbug.com/41490094
    public void testCameraBlocked() throws Exception {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Camera",
                        SiteSettingsCategory.Type.CAMERA,
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the camera permission doesn't get requested.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                0,
                /* withGesture= */ true,
                /* isDialog= */ true);
    }

    /** Sets Allow Camera Enabled to be true and make sure it is set correctly. */
    @DisabledTest(message = "https://crbug.com/429083114")
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // crbug.com/41490094
    public void testCameraNotBlocked() throws Exception {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Camera",
                        SiteSettingsCategory.Type.CAMERA,
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                0,
                /* withGesture= */ true,
                /* isDialog= */ true);
    }

    /** Sets Allow Mic Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicBlocked() throws Exception {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Mic",
                        SiteSettingsCategory.Type.MICROPHONE,
                        ContentSettingsType.MEDIASTREAM_MIC,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the microphone permission doesn't get requested.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                0,
                true,
                true);
    }

    /** Sets Allow Mic Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisabledTest(message = "crbug.com/41490094 && crbug.com/425926397")
    public void testMicNotBlocked() throws Exception {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Mic",
                        SiteSettingsCategory.Type.MICROPHONE,
                        ContentSettingsType.MEDIASTREAM_MIC,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Launch a page that uses the microphone and make sure a permission prompt shows up.
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                0,
                true,
                true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowBackgroundSync() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BackgroundSync",
                        SiteSettingsCategory.Type.BACKGROUND_SYNC,
                        ContentSettingsType.BACKGROUND_SYNC,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockBackgroundSync() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BackgroundSync",
                        SiteSettingsCategory.Type.BACKGROUND_SYNC,
                        ContentSettingsType.BACKGROUND_SYNC,
                        false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowNotifications() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Notifications",
                        SiteSettingsCategory.Type.NOTIFICATIONS,
                        ContentSettingsType.NOTIFICATIONS,
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.NOTIFICATIONS_TRI_STATE_PREF_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockNotifications() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Notifications",
                        SiteSettingsCategory.Type.NOTIFICATIONS,
                        ContentSettingsType.NOTIFICATIONS,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowGeolocation() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Geolocation",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.LOCATION_TRI_STATE_PREF_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockGeolocation() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "Geolocation",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    void setGeolocationSetting(String url, GeolocationSetting setting) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebsitePreferenceBridgeJni.get()
                                .setGeolocationSettingForOrigin(
                                        getBrowserContextHandle(),
                                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                                        url,
                                        "*",
                                        setting.mApproximate,
                                        setting.mPrecise));
    }

    GeolocationSetting getGeolocationSetting(String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebsitePreferenceBridgeJni.get()
                                .getGeolocationSettingForOrigin(
                                        getBrowserContextHandle(),
                                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                                        url,
                                        "https://example.com"));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION
    })
    public void testRemoveGeolocationWithOptions() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var allowSetting = new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        setGeolocationSetting(url, allowSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(allowSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Remove")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK),
                getGeolocationSetting(url));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION
    })
    // TODO(crbug.com/433576895): Re-enable containment feature once the test is fixed.
    @DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testChangeGeolocationWithOptions() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var allowSetting = new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        setGeolocationSetting(url, allowSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(allowSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Approximate")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK),
                getGeolocationSetting(url));

        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Block")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK),
                getGeolocationSetting(url));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION
    })
    public void testChangeGeolocationWithOptionsRadioButtonsEnabledState() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var blockSetting = new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK);
        setGeolocationSetting(url, blockSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(blockSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());

        // Verify that the radio buttons for location precision are disabled when 'Block' is
        // selected.
        onView(withText("Precise")).check(matches(not(isEnabled())));
        onView(withText("Approximate")).check(matches(not(isEnabled())));

        // Click 'Allow' and verify that the radio buttons for location precision are enabled.
        onView(withText("Allow")).perform(click());
        onView(withText("Precise")).check(matches(isEnabled()));
        onView(withText("Approximate")).check(matches(isEnabled()));

        // Click 'Block' again and verify that the radio buttons for location precision are
        // disabled.
        onView(withText("Block")).perform(click());
        onView(withText("Precise")).check(matches(not(isEnabled())));
        onView(withText("Approximate")).check(matches(not(isEnabled())));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION
    })
    public void testEmbargoedGeolocationWithOptions() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final String url = mPermissionRule.getURL("/chrome/test/data/geolocation/simple.html");
        final String origin = Origin.create(url).toString();
        triggerEmbargoForOrigin(url);
        assertEquals(
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK),
                getGeolocationSetting(url));

        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText("Automatically blocked")).check(matches(isDisplayed()));
        onView(withText(origin)).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Allow")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW),
                getGeolocationSetting(url));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowUsb() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockUsb() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowSerialPort() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "SerialPort",
                        SiteSettingsCategory.Type.SERIAL_PORT,
                        ContentSettingsType.SERIAL_GUARD,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockSerialPort() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "SerialPort",
                        SiteSettingsCategory.Type.SERIAL_PORT,
                        ContentSettingsType.SERIAL_GUARD,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowAutomaticDownloads() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutomaticDownloads",
                        SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                        ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockAutomaticDownloads() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutomaticDownloads",
                        SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                        ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowBluetoothScanningWithToggle() {
        new TwoStatePermissionTestCaseWithToggle(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowBluetoothScanning() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockBluetoothScanningWithToggle() {
        new TwoStatePermissionTestCaseWithToggle(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockBluetoothScanning() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowBluetoothGuardWithToggle() {
        new TwoStatePermissionTestCaseWithToggle(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowBluetoothGuard() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockBluetoothGuardWithToggle() {
        new TwoStatePermissionTestCaseWithToggle(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockBluetoothGuard() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new TwoStatePermissionTestCaseWithRadioButton(
                        "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new TwoStatePermissionTestCaseWithRadioButton(
                        "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAntiAbuse() {
        new TwoStatePermissionTestCaseWithToggle(
                        "AntiAbuse",
                        SiteSettingsCategory.Type.ANTI_ABUSE,
                        ContentSettingsType.ANTI_ABUSE,
                        true)
                .withExpectedPrefKeys(ANTI_ABUSE_PREF_KEYS)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAntiAbuse() {
        new TwoStatePermissionTestCaseWithToggle(
                        "AntiAbuse",
                        SiteSettingsCategory.Type.ANTI_ABUSE,
                        ContentSettingsType.ANTI_ABUSE,
                        false)
                .withExpectedPrefKeys(ANTI_ABUSE_PREF_KEYS)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowAr() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AR",
                        SiteSettingsCategory.Type.AUGMENTED_REALITY,
                        ContentSettingsType.AR,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockAr() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AR",
                        SiteSettingsCategory.Type.AUGMENTED_REALITY,
                        ContentSettingsType.AR,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowVr() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "VR",
                        SiteSettingsCategory.Type.VIRTUAL_REALITY,
                        ContentSettingsType.VR,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockVr() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "VR",
                        SiteSettingsCategory.Type.VIRTUAL_REALITY,
                        ContentSettingsType.VR,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowHandTracking() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "HandTracking",
                        SiteSettingsCategory.Type.HAND_TRACKING,
                        ContentSettingsType.HAND_TRACKING,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockHandTrackingWithToggle() {
        new TwoStatePermissionTestCaseWithToggle(
                        "HandTracking",
                        SiteSettingsCategory.Type.HAND_TRACKING,
                        ContentSettingsType.HAND_TRACKING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockHandTracking() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "HandTracking",
                        SiteSettingsCategory.Type.HAND_TRACKING,
                        ContentSettingsType.HAND_TRACKING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowIdleDetection() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "IdleDetection",
                        SiteSettingsCategory.Type.IDLE_DETECTION,
                        ContentSettingsType.IDLE_DETECTION,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockIdleDetection() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "IdleDetection",
                        SiteSettingsCategory.Type.IDLE_DETECTION,
                        ContentSettingsType.IDLE_DETECTION,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowLocalNetworkAccess() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "LocalNetworkAccess",
                        SiteSettingsCategory.Type.LOCAL_NETWORK_ACCESS,
                        ContentSettingsType.LOCAL_NETWORK_ACCESS,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockLocalNetworkAccess() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "LocalNetworkAccess",
                        SiteSettingsCategory.Type.LOCAL_NETWORK_ACCESS,
                        ContentSettingsType.LOCAL_NETWORK_ACCESS,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowWindowManager() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "WindowManagement",
                        SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                        ContentSettingsType.WINDOW_MANAGEMENT,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockWindowManager() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "WindowManagement",
                        SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                        ContentSettingsType.WINDOW_MANAGEMENT,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    public void testAllowAutoPictureInPicture() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutoPictureInPicture",
                        SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                        ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({
        MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID,
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON
    })
    public void testBlockAutoPictureInPicture() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutoPictureInPicture",
                        SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                        ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testAutoPiPPermissionNotVisibleWhenDisabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNull(websitePreferences.findPreference("auto_picture_in_picture"));
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowAutoDarkWithToggle() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Enabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCaseWithToggle(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Enabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockAutoDarkWithToggle() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Disabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCaseWithToggle(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        false)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Disabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCaseWithRadioButton(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        false)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowRequestDesktopSite() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "RequestDesktopSite",
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                        ContentSettingsType.REQUEST_DESKTOP_SITE,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.DESKTOP_SITE_WINDOW_TOGGLE_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockRequestDesktopSite() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "RequestDesktopSite",
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                        ContentSettingsType.REQUEST_DESKTOP_SITE,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowFederatedIdentityApi() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "FederatedIdentityApi",
                        SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                        ContentSettingsType.FEDERATED_IDENTITY_API,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockFederatedIdentityApi() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "FederatedIdentityApi",
                        SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                        ContentSettingsType.FEDERATED_IDENTITY_API,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllowJavascriptOptimizer() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "JavascriptOptimizer",
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                        ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testBlockJavascriptOptimizer() {
        new TwoStatePermissionTestCaseWithRadioButton(
                        "JavascriptOptimizer",
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                        ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOsBlocksJavascriptOptimizerWithToggle() {
        String pageOrigin = mPermissionRule.getOrigin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            new GURL(pageOrigin),
                            new GURL(pageOrigin),
                            ContentSetting.ALLOW);
                });

        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_TOGGLE_KEY,
                                SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY,
                                SingleCategorySettings.ALLOWED_GROUP,
                                SingleCategorySettings.ADD_EXCEPTION_KEY,
                            });

                    ChromeSwitchPreference binaryToggle =
                            (ChromeSwitchPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_TOGGLE_KEY);
                    Assert.assertFalse(binaryToggle.isChecked());
                    Assert.assertFalse(binaryToggle.isEnabled());

                    Preference toggleDisableReason =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY);
                    Assert.assertEquals(
                            AdvancedProtectionTestRule.TEST_JAVASCRIPT_OPTIMIZER_MESSAGE,
                            toggleDisableReason.getTitle());

                    settingsActivity.finish();
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testOsBlocksJavascriptOptimizer() {
        String pageOrigin = mPermissionRule.getOrigin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            new GURL(pageOrigin),
                            new GURL(pageOrigin),
                            ContentSetting.ALLOW);
                });

        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY,
                                SingleCategorySettings.ALLOWED_GROUP,
                                SingleCategorySettings.ADD_EXCEPTION_KEY,
                            });

                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    Assert.assertFalse(binaryRadioButton.isChecked());
                    Assert.assertFalse(binaryRadioButton.isEnabled());

                    Preference radioButtonDisableReason =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY);
                    Assert.assertEquals(
                            AdvancedProtectionTestRule.TEST_JAVASCRIPT_OPTIMIZER_MESSAGE,
                            radioButtonDisableReason.getTitle());

                    settingsActivity.finish();
                });
    }

    // Due to bug DefaultPassthroughCommandDecoder feature needs to be on whenever
    // BaseSwitches.ENABLE_LOW_END_DEVICE_MODE feature is on to avoid crash.
    // See https://issues.chromium.org/448715624
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({
        ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON,
        "DefaultPassthroughCommandDecoder"
    })
    public void testAddingJavascriptOptimizerExceptionsBlockedIfNotEnoughRam() {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_DISABLED_REASON_KEY,
                            });

                    Preference addExceptionButton =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_KEY);
                    Assert.assertFalse(addExceptionButton.isEnabled());

                    Preference addExceptionButtonDisabledReason =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_DISABLED_REASON_KEY);
                    Context context = ApplicationProvider.getApplicationContext();
                    int expectedReasonId =
                            R.string.website_settings_js_opt_add_exceptions_disabled_reason;
                    Assert.assertEquals(
                            context.getString(expectedReasonId),
                            addExceptionButtonDisabledReason.getTitle());

                    settingsActivity.finish();
                });
    }

    /**
     * Test that if the Javascript-optimizer is enabled by enterprise policy but disabled by the OS
     * advanced-portection-mode setting that the enterprise policy is given precedence.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @Policies.Add({@Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "1")})
    public void testPolicyHigherPriorityThanOsBlockingJavascriptOptimizerWithToggle() {
        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_TOGGLE_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_KEY
                            });

                    ChromeSwitchPreference binaryToggle =
                            (ChromeSwitchPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_TOGGLE_KEY);
                    Assert.assertTrue(binaryToggle.isChecked());
                    Assert.assertFalse(binaryToggle.isEnabled());

                    Preference addExceptionPreference =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_KEY);
                    Assert.assertFalse(addExceptionPreference.isEnabled());

                    // Proabably never worked. crbug.com/446200399
                    // onData(withKey(SingleCategorySettings.ALLOWED_GROUP))
                    //         .inAdapterView(
                    //                 allOf(
                    //                         withContentDescription(
                    //                                 R.string.managed_by_your_organization),
                    //                         withText(R.string.managed_by_your_organization)))
                    //         .check(matches(isDisplayed()));

                    settingsActivity.finish();
                });
    }

    /**
     * Test that if the Javascript-optimizer is enabled by enterprise policy but disabled by the OS
     * advanced-portection-mode setting that the enterprise policy is given precedence.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    @Policies.Add({@Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "1")})
    public void testPolicyHigherPriorityThanOsBlockingJavascriptOptimizer() {
        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_KEY
                            });

                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    Assert.assertTrue(binaryRadioButton.isChecked());
                    Assert.assertFalse(binaryRadioButton.isEnabled());

                    Preference addExceptionPreference =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_KEY);
                    Assert.assertFalse(addExceptionPreference.isEnabled());

                    // Proabably never worked. crbug.com/446200399
                    // onData(withKey(SingleCategorySettings.ALLOWED_GROUP))
                    //         .inAdapterView(
                    //                 allOf(
                    //                         withContentDescription(
                    //                                 R.string.managed_by_your_organization),
                    //                         withText(R.string.managed_by_your_organization)))
                    //         .check(matches(isDisplayed()));

                    settingsActivity.finish();
                });
    }

    /**
     * Test that when: (1) Javascript-optimizer permission toggle is present on the {@link
     * SingleWebsiteSettings} screen AND (2) Advanced protection is requested by the operating
     * system THAT the toggle is still enabled because explicit Javascript-optimizer content
     * settings have priority over advanced-protection-mode.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOsBlocksJavascriptOptimizerSingleWebsite() throws Exception {
        final String pageUrl = mPermissionRule.getURL("/chrome/test/data/android/simple.html");
        String pageOrigin = mPermissionRule.getOrigin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            new GURL(pageOrigin),
                            new GURL(pageOrigin),
                            ContentSetting.ALLOW);
                });

        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(pageUrl));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    Preference javascriptOptimizerPreference =
                            websitePreferences.findPreference("javascript_optimizer");
                    Assert.assertTrue(javascriptOptimizerPreference.isEnabled());
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    // Auto does not have actions to handle ACTION_CHANNEL_NOTIFICATION_SETTINGS
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testEmbargoedNotificationSiteSettings() throws Exception {
        final String url =
                mPermissionRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(url);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(url));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();

                    final Preference notificationPreference =
                            websitePreferences.findPreference("push_notifications_list");

                    Assert.assertEquals(
                            context.getString(R.string.automatically_blocked),
                            notificationPreference.getSummary());
                    websitePreferences.launchOsChannelSettingsFromPreference(
                            notificationPreference);
                });
        // There are lots of native posted tasks since start up. So we need to wait for
        // all tasks to settle.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Channel was not found",
                            getChannelId(url),
                            not(ChromeChannelDefinitions.ChannelId.SITES));
                });
        // Close the OS notification settings UI.
        UiAutomatorUtils.getInstance().pressBack();
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1094934")
    public void testEmbargoedNotificationCategorySiteSettings() throws Exception {
        final String urlToEmbargo =
                mPermissionRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(urlToEmbargo);

        final String urlToBlock =
                mPermissionRule.getURLWithHostName(
                        "exampleToBlock.com",
                        "/chrome/test/data/notifications/notification_tester.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .setPermissionSettingForOrigin(
                                    getBrowserContextHandle(),
                                    ContentSettingsType.NOTIFICATIONS,
                                    urlToBlock,
                                    urlToBlock,
                                    ContentSetting.BLOCK);
                });

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.NOTIFICATIONS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean blockedByEmbargo =
                            WebsitePreferenceBridgeJni.get()
                                    .isNotificationEmbargoedForOrigin(
                                            getBrowserContextHandle(), urlToEmbargo);
                    Assert.assertTrue(blockedByEmbargo);

                    final String blockedGroupKey = "blocked_group";
                    // Click on Blocked group in Category Settings. By default Blocked is closed, to
                    // be able to find any origins inside, Blocked should be opened.
                    SingleCategorySettings websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    websitePreferences.findPreference(blockedGroupKey).performClick();

                    // After triggering onClick on Blocked group, all UI will be discarded and
                    // reinitialized from scratch. Init all variables again, otherwise it will use
                    // stale information.
                    websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    ExpandablePreferenceGroup blockedGroup =
                            (ExpandablePreferenceGroup)
                                    websitePreferences.findPreference(blockedGroupKey);

                    Assert.assertTrue(blockedGroup.isExpanded());
                    // Only |url| has been added under embargo.
                    Assert.assertEquals(2, blockedGroup.getPreferenceCount());

                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.automatically_blocked),
                            blockedGroup.getPreference(0).getSummary());

                    // Blocked origin should has no summary.
                    assertNull(blockedGroup.getPreference(1).getSummary());
                });
        settingsActivity.finish();
    }

    /**
     * Test that embargoing federated identity permission displays "Automatically Blocked" message
     * in page info UI. Federated identity is a content setting. Content settings use a different
     * code path than permissions (like notifications).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testEmbargoedFederatedIdentity() throws Exception {
        final String rpUrl =
                mPermissionRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/android/simple.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FederatedIdentityTestUtils.embargoFedCmForRelyingParty(new GURL(rpUrl));
                });

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(rpUrl));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    final Preference fedCmPreference =
                            websitePreferences.findPreference("federated_identity_api_list");

                    Assert.assertEquals(
                            context.getString(R.string.automatically_blocked),
                            fedCmPreference.getSummary());
                });
        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentDefaultOption() throws Exception {
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskAllow() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);

        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runDenyTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1269556,https://crbug.com/1414569,crbug.com/1234530")
    public void testProtectedContentAllowThenBlock() throws Exception {
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);

        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true,
                true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testDesktopSiteWindowSettingsWithToggle() {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HistogramWatcher histogramExpectation =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "Android.RequestDesktopSite.WindowSettingChanged", true);
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    // Window setting is only available when the Global Setting is ON.
                    ChromeSwitchPreference toggle =
                            preferences.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
                    preferences.onPreferenceChange(toggle, true);

                    ChromeBaseCheckBoxPreference windowSettingPref =
                            preferences.findPreference(
                                    SingleCategorySettings.DESKTOP_SITE_WINDOW_TOGGLE_KEY);
                    PrefService prefService = UserPrefs.get(getBrowserContextHandle());
                    preferences.onPreferenceChange(windowSettingPref, true);
                    Assert.assertTrue(
                            "Window setting should be ON.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                    histogramExpectation.assertExpected();

                    preferences.onPreferenceChange(windowSettingPref, false);
                    Assert.assertFalse(
                            "Window setting should be OFF.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testDesktopSiteWindowSettings() {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HistogramWatcher histogramExpectation =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "Android.RequestDesktopSite.WindowSettingChanged", true);
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    // Window setting is only available when the Global Setting is ON.
                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    preferences.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    preferences.onPreferenceChange(binaryRadioButton, true);

                    ChromeBaseCheckBoxPreference windowSettingPref =
                            preferences.findPreference(
                                    SingleCategorySettings.DESKTOP_SITE_WINDOW_TOGGLE_KEY);
                    PrefService prefService = UserPrefs.get(getBrowserContextHandle());
                    preferences.onPreferenceChange(windowSettingPref, true);
                    Assert.assertTrue(
                            "Window setting should be ON.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                    histogramExpectation.assertExpected();

                    preferences.onPreferenceChange(windowSettingPref, false);
                    Assert.assertFalse(
                            "Window setting should be OFF.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutorevokePermissionsSwitch() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SiteSettings.PERMISSION_AUTOREVOCATION_HISTOGRAM_NAME, true);
        // Set the initial toggle state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(getBrowserContextHandle())
                            .setBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED, false);
                });

        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");

        // Scroll to permission autorevocation preference and click it.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(
                                                R.string.safety_hub_autorevocation_toggle_title))));
        onView(withText(R.string.safety_hub_autorevocation_toggle_title))
                .check(matches(not(isChecked())))
                .perform(click());

        // Verify that the pref has been correctly set.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Unused site permission revocation should be enabled.",
                            UserPrefs.get(getBrowserContextHandle())
                                    .getBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED));
                });
        histogramExpectation.assertExpected();

        settingsActivity.finish();
    }

    private void renderSettingsPage(SettingsActivity settingsActivity, String name)
            throws IOException {
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, name);
        settingsActivity.finish();
    }

    private void renderCategoryPage(@SiteSettingsCategory.Type int category, String name)
            throws IOException {
        var settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(category);
        renderSettingsPage(settingsActivity, name);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderStorageAccessPage() throws Exception {
        createStorageAccessExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.STORAGE_ACCESS, "site_settings_storage_access_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderStorageAccessSubpage() throws Exception {
        createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startStorageAccessSettingsActivity(getStorageAccessSite());
        renderSettingsPage(settingsActivity, "site_settings_storage_access_subpage");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void renderRwsSingleWebsiteSettings() throws Exception {
        createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(getRwsOwnerSite());
        renderSettingsPage(settingsActivity, "site_settings_rws_single_website");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderRwsGroupedWebsiteSettings() throws Exception {
        createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startGroupedWebsitesPreferences(getRwsSiteGroup());
        renderSettingsPage(settingsActivity, "site_settings_rws_grouped_website");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderSiteDataPage() throws Exception {
        createCookieExceptions();
        renderCategoryPage(SiteSettingsCategory.Type.SITE_DATA, "site_settings_site_data_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderThirdPartyCookiesPageWithFps() throws Exception {
        createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Policies.Add({@Policies.Item(key = "BlockThirdPartyCookies", string = "true")})
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
        createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, "site_settings_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    public void testRenderLocationPage() throws Exception {
        createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.DEVICE_LOCATION, "site_settings_location_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderProtectedMediaPage() throws Exception {
        createCookieExceptions();
        renderCategoryPage(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, "site_settings_protected_media_page");
    }

    /** Test case for checking that settings with binary toggles are disabled by policy. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultJavaScriptSetting", string = "2"),
        @Policies.Item(key = "DefaultPopupsSetting", string = "2"),
        @Policies.Item(key = "DefaultGeolocationSetting", string = "2"),
        @Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "2")
    })
    @DisableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllTwoStateToggleDisabledByPolicyWithToggle() {
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.POPUPS);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.DEVICE_LOCATION);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);
        // TODO(crbug.com/40879457): add a test for sensors once crash in the sensors settings page
        // is resolved.
    }

    /** Test case for checking that settings with binary toggles are disabled by policy. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultJavaScriptSetting", string = "2"),
        @Policies.Item(key = "DefaultPopupsSetting", string = "2"),
        @Policies.Item(key = "DefaultGeolocationSetting", string = "2"),
        @Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "2")
    })
    @EnableFeatures(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
    public void testAllTwoStateToggleDisabledByPolicy() {
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.POPUPS);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.DEVICE_LOCATION);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);
        // TODO(crbug.com/40879457): add a test for sensors once crash in the sensors settings page
        // is resolved.
    }

    private void testTwoStateToggleDisabledByPolicy(@SiteSettingsCategory.Type int type) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);
        SingleCategorySettings singleCategorySettings =
                (SingleCategorySettings) settingsActivity.getMainFragment();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PERMISSION_SITE_SETTING_RADIO_BUTTON)
                && type != SiteSettingsCategory.Type.ANTI_ABUSE) {
            BinaryStatePermissionPreference binaryRadioButton =
                    singleCategorySettings.findPreference(
                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);

            Assert.assertFalse(binaryRadioButton.isEnabled());
        } else {
            ChromeSwitchPreference binaryToggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);

            Assert.assertFalse(binaryToggle.isEnabled());
        }

        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    /**
     * Allows third party cookies for a website, and tests that the UI shows a managed preference in
     * the allowed group. Checks that it shows the toast when the preference is clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "CookiesAllowedForUrls", string = "[\"[*.]chromium.org\"]")
    })
    public void testAllowCookiesForUrl() throws Exception {
        testCookiesSettingsManagedForUrl(SingleCategorySettings.ALLOWED_GROUP);
    }

    /**
     * Blocks third party cookies for a website, and tests that the UI shows a managed preference in
     * the blocked group. Checks that it shows toast when the preference is clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "CookiesBlockedForUrls", string = "[\"[*.]chromium.org\"]")
    })
    public void testBlockCookiesForUrl() throws Exception {
        testCookiesSettingsManagedForUrl(SingleCategorySettings.BLOCKED_GROUP);
    }

    public void testCookiesSettingsManagedForUrl(String setting) throws Exception {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.SITE_DATA);

        SingleCategorySettings websitePreferences =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        ExpandablePreferenceGroup managedGroup =
                (ExpandablePreferenceGroup) websitePreferences.findPreference(setting);
        Assert.assertTrue("The blocked group should be expanded.", managedGroup.isExpanded());
        Assert.assertEquals(
                "The blocked expandable group should have exactly one website listed.",
                1,
                managedGroup.getPreferenceCount());
        ChromeImageViewPreference websitePreference =
                (ChromeImageViewPreference) managedGroup.getPreference(0);

        /*
         * Swipes to the end of the screen to show the website preference for the blocked site
         * then checks that the content description and the summary text reflect the managed state.
         */
        onView(ViewMatchers.withId(android.R.id.content)).perform(swipeUp());
        // Proabably never worked. crbug.com/446200399
        // onData(withKey(setting))
        //         .inAdapterView(
        //                 allOf(
        //                         withContentDescription(R.string.managed_by_your_organization),
        //                         withText(R.string.managed_by_your_organization)))
        //         .check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    websitePreference.performClick();
                });
        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(allOf(withId(R.id.toast_text))))
                .check(matches(isDisplayed()));
    }

    static class PermissionTestCase {
        protected final String mTestName;
        protected final @SiteSettingsCategory.Type int mSiteSettingsType;
        protected final @ContentSettingsType.EnumType int mContentSettingsType;
        protected final boolean mIsCategoryEnabled;
        protected final List<String> mExpectedPreferenceKeys;

        protected SettingsActivity mSettingsActivity;

        PermissionTestCase(
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
        PermissionTestCase withExpectedPrefKeys(String expectedPrefKeys) {
            mExpectedPreferenceKeys.add(expectedPrefKeys);
            return this;
        }

        PermissionTestCase withExpectedPrefKeysAtStart(String expectedPrefKeys) {
            mExpectedPreferenceKeys.add(0, expectedPrefKeys);
            return this;
        }

        PermissionTestCase withExpectedPrefKeys(String[] expectedPrefKeys) {
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

    /** Test case for site settings with a global toggle. */
    static class TwoStatePermissionTestCaseWithToggle extends PermissionTestCase {
        TwoStatePermissionTestCaseWithToggle(
                String testName, int siteSettingsType, int contentSettingsType, boolean enabled) {
            super(testName, siteSettingsType, contentSettingsType, enabled);
            mExpectedPreferenceKeys.add(SingleCategorySettings.BINARY_TOGGLE_KEY);
        }

        @Override
        public void doTest(SingleCategorySettings singleCategorySettings) {
            // Verify toggle related checks first as they may affect the preferences on the screen.
            assertToggleTitleAndSummary(singleCategorySettings);
            assertGlobalToggleForCategory(singleCategorySettings);

            super.doTest(singleCategorySettings);
        }

        /** Verify {@link SingleCategorySettings} is wired correctly. */
        private void assertGlobalToggleForCategory(SingleCategorySettings singleCategorySettings) {
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

            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            assertNotNull("Toggle should not be null.", toggle);

            singleCategorySettings.onPreferenceChange(toggle, mIsCategoryEnabled);
            Assert.assertEquals(
                    exceptionString,
                    mIsCategoryEnabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), mContentSettingsType));
        }

        /** Verify {@link ContentSettingsResources} is set correctly. */
        private void assertToggleTitleAndSummary(SingleCategorySettings singleCategorySettings) {
            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            assertThat(toggle).isNotNull();

            Assert.assertEquals(
                    "Preference title is not set correctly.",
                    singleCategorySettings
                            .getResources()
                            .getString(ContentSettingsResources.getTitle(mContentSettingsType)),
                    toggle.getTitle());
            assertNotNull("Enabled summary text should not be null.", toggle.getSummaryOn());
            assertNotNull("Disabled summary text should not be null.", toggle.getSummaryOff());

            String summary =
                    mIsCategoryEnabled
                            ? toggle.getSummaryOn().toString()
                            : toggle.getSummaryOff().toString();
            String expected =
                    singleCategorySettings
                            .getResources()
                            .getString(
                                    mIsCategoryEnabled
                                            ? ContentSettingsResources.getEnabledSummary(
                                                    mContentSettingsType)
                                            : ContentSettingsResources.getDisabledSummary(
                                                    mContentSettingsType));
            Assert.assertEquals(
                    "Summary text in state <" + mIsCategoryEnabled + "> does not match.",
                    expected,
                    summary);
        }
    }

    /** Test case for site settings with a global radio button group. */
    static class TwoStatePermissionTestCaseWithRadioButton extends PermissionTestCase {
        TwoStatePermissionTestCaseWithRadioButton(
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
            BinaryStatePermissionPreference radio_button =
                    singleCategorySettings.findPreference(
                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
            assertThat(radio_button).isNotNull();

            Assert.assertEquals(
                    "Preference text is not set correctly.",
                    ContentSettingsResources.getBinaryStateSettingResourceIDs(mContentSettingsType)[
                            0],
                    radio_button.getDescriptionIds()[0]);
            Assert.assertEquals(
                    "Preference text is not set correctly.",
                    ContentSettingsResources.getBinaryStateSettingResourceIDs(mContentSettingsType)[
                            1],
                    radio_button.getDescriptionIds()[1]);
        }
    }

    private static String getChannelId(String url) {
        PayloadCallbackHelper<String> helper = new PayloadCallbackHelper();
        SiteChannelsManager.getInstance()
                .getChannelIdForOriginAsync(
                        Origin.createOrThrow(url).toString(), helper::notifyCalled);
        return helper.getOnlyPayloadBlocking();
    }
}
