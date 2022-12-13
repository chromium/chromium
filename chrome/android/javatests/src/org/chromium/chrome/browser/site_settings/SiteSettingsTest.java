// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.PreferenceMatchers.withKey;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;

import static org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_DISPLAY_SETTING_ENABLED;
import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.FederatedIdentityTestUtils;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsFeatureList;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.FPSCookieSettings;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;
import org.chromium.components.browser_ui.site_settings.R;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.TriStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.TriStateSiteSettingsPreference;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Tests for everything under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
@Batch(SiteSettingsTest.SITE_SETTINGS_BATCH_NAME)
public class SiteSettingsTest {
    public static final String SITE_SETTINGS_BATCH_NAME = "site_settings";

    @ClassRule
    public static PermissionTestRule mPermissionRule = new PermissionTestRule(true);

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mPermissionRule, false);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @Mock
    private SettingsLauncher mSettingsLauncher;

    private PermissionUpdateWaiter mPermissionUpdateWaiter;

    private static final String[] NULL_ARRAY = new String[0];
    private static final String[] BINARY_TOGGLE = new String[] {"binary_toggle"};
    private static final String[] BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT =
            new String[] {"info_text", "binary_toggle", "add_exception"};
    private static final String[] BINARY_TOGGLE_WITH_EXCEPTION =
            new String[] {"binary_toggle", "add_exception"};
    private static final String[] BINARY_TOGGLE_WITH_OS_WARNING_EXTRA =
            new String[] {"binary_toggle", "os_permissions_warning_extra"};
    private static final String[] CLEAR_BROWSING_DATA_LINK =
            new String[] {"clear_browsing_data_link", "clear_browsing_divider"};

    @Before
    public void setUp() throws TimeoutException {
        // Clean up cookies and permissions to ensure tests run in a clean environment.
        cleanUpCookiesAndPermissions();
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() throws TimeoutException {
        if (mPermissionUpdateWaiter != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mPermissionRule.getActivity().getActivityTab().removeObserver(
                        mPermissionUpdateWaiter);
            });
        }

        // Clean up default content setting and system settings.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int t = 0; t < SiteSettingsCategory.Type.NUM_ENTRIES; t++) {
                if (SiteSettingsCategory.contentSettingsType(t) >= 0) {
                    WebsitePreferenceBridge.setDefaultContentSetting(getBrowserContextHandle(),
                            SiteSettingsCategory.contentSettingsType(t),
                            ContentSettingValues.DEFAULT);
                }
            }
        });
        LocationUtils.setFactory(null);
        LocationProviderOverrider.setLocationProviderImpl(null);
        NfcSystemLevelSetting.resetNfcForTesting();
        IncognitoUtils.setEnabledForTesting(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY)
                .apply();
    }

    @AfterClass
    public static void tearDownAfterClass() throws TimeoutException {
        cleanUpCookiesAndPermissions();
    }

    private static BrowserContextHandle getBrowserContextHandle() {
        return Profile.getLastUsedRegularProfile();
    }

    private void initializeUpdateWaiter(final boolean expectGranted) {
        if (mPermissionUpdateWaiter != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mPermissionRule.getActivity().getActivityTab().removeObserver(
                        mPermissionUpdateWaiter);
            });
        }
        Tab tab = mPermissionRule.getActivity().getActivityTab();

        mPermissionUpdateWaiter = new PermissionUpdateWaiter(
                expectGranted ? "Granted" : "Denied", mPermissionRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(mPermissionUpdateWaiter));
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
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mPermissionRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    private static void cleanUpCookiesAndPermissions() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.COOKIES, BrowsingDataType.SITE_SETTINGS},
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
                        == SettingsFeatureList.isEnabled(
                                SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
                ? allOf(withId(R.id.managed_disclaimer_text),
                        hasSibling(withId(R.id.radio_button_layout)))
                : withId(R.id.managed_view_legacy);
    }

    private void createCookieExceptions() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingCustomScope(getBrowserContextHandle(),
                    ContentSettingsType.COOKIES, "*", "secondary.com", ContentSettingValues.ALLOW);
            WebsitePreferenceBridge.setContentSettingCustomScope(getBrowserContextHandle(),
                    ContentSettingsType.COOKIES, "primary.com", "*", ContentSettingValues.ALLOW);
        });
    }

    /**
     * Sets Allow Location Enabled to be true and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCase("Location", SiteSettingsCategory.Type.DEVICE_LOCATION,
                ContentSettingsType.GEOLOCATION, true)
                .run();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue("Location should be allowed.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        initializeUpdateWaiter(true /* expectGranted */);

        // Launch a page that uses geolocation and make sure a permission prompt shows up.
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html", "", 0, false, true);
    }

    /**
     * Sets Allow Location Enabled to be false and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationNotEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new TwoStatePermissionTestCase("Location", SiteSettingsCategory.Type.DEVICE_LOCATION,
                ContentSettingsType.GEOLOCATION, false)
                .run();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("Location should be blocked.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        getBrowserContextHandle())));

        // Launch a page that uses geolocation. No permission prompt is expected.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html", "", 0, false, true);
    }

    private void setCookiesEnabled(final SettingsActivity settingsActivity, final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategorySettings websitePreferences =
                        (SingleCategorySettings) settingsActivity.getMainFragment();
                final FourStateCookieSettingsPreference cookies =
                        (FourStateCookieSettingsPreference) websitePreferences.findPreference(
                                SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(
                        cookies, enabled ? CookieSettingsState.ALLOW : CookieSettingsState.BLOCK);
                Assert.assertEquals("Cookies should be " + (enabled ? "allowed" : "blocked"),
                        doesAcceptCookies(), enabled);
            }

            private boolean doesAcceptCookies() {
                return WebsitePreferenceBridge.isCategoryEnabled(
                        getBrowserContextHandle(), ContentSettingsType.COOKIES);
            }
        });
    }

    private void setBlockCookiesSiteException(final SettingsActivity settingsActivity,
            final String hostname, final boolean thirdPartiesOnly) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategorySettings websitePreferences =
                        (SingleCategorySettings) settingsActivity.getMainFragment();

                Assert.assertTrue(doesAcceptCookies());
                if (thirdPartiesOnly) {
                    websitePreferences.onAddSite(SITE_WILDCARD, hostname);
                } else {
                    websitePreferences.onAddSite(hostname, SITE_WILDCARD);
                }
            }

            private boolean doesAcceptCookies() {
                return WebsitePreferenceBridge.isCategoryEnabled(
                        getBrowserContextHandle(), ContentSettingsType.COOKIES);
            }

        });
    }

    private enum ToggleButtonState { EnabledUnchecked, EnabledChecked, Disabled }

    /**
     * Checks if the button representing the given state matches the managed expectation.
     */
    private void checkFourStateCookieToggleButtonState(final SettingsActivity settingsActivity,
            final CookieSettingsState state, final ToggleButtonState toggleState) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    (FourStateCookieSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);
            boolean enabled = toggleState != ToggleButtonState.Disabled;
            boolean checked = toggleState == ToggleButtonState.EnabledChecked;
            Assert.assertEquals(state + " button should be " + (enabled ? "enabled" : "disabled"),
                    enabled, fourStateCookieToggle.isButtonEnabledForTesting(state));
            Assert.assertEquals(state + " button should be " + (checked ? "checked" : "unchecked"),
                    checked, fourStateCookieToggle.isButtonCheckedForTesting(state));
        });
    }

    private void checkDefaultCookiesSettingManaged(boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Default Cookie Setting should be " + (expected ? "managed" : "unmanaged"),
                    expected,
                    WebsitePreferenceBridge.isContentSettingManaged(
                            getBrowserContextHandle(), ContentSettingsType.COOKIES));
        });
    }

    private void checkThirdPartyCookieBlockingManaged(boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Third Party Cookie Blocking should be " + (expected ? "managed" : "unmanaged"),
                    expected,
                    UserPrefs.get(Profile.getLastUsedRegularProfile())
                            .isManagedPreference(COOKIE_CONTROLS_MODE));
        });
    }

    private void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
                TriStateCookieSettingsPreference preference =
                        preferences.findPreference(SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);
                preferences.onPreferenceChange(preference,
                        enabled ? CookieControlsMode.OFF : CookieControlsMode.BLOCK_THIRD_PARTY);
            } else {
                ChromeSwitchPreference toggle =
                        preferences.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
                preferences.onPreferenceChange(toggle, enabled);
            }
        });
        if (type == SiteSettingsCategory.Type.SITE_DATA && !enabled) {
            int id = R.string.website_settings_site_data_page_block_confirm_dialog_confirm_button;
            onViewWaiting(withText(id)).perform(click());
        }
        settingsActivity.finish();
    }

    private void setGlobalTriStateToggleForCategory(
            final @SiteSettingsCategory.Type int type, final int newValue) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            TriStateSiteSettingsPreference triStateToggle =
                    (TriStateSiteSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.TRI_STATE_TOGGLE_KEY);
            preferences.onPreferenceChange(triStateToggle, newValue);
        });
        settingsActivity.finish();
    }

    private void setFourStateCookieToggle(CookieSettingsState newState) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    (FourStateCookieSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);
            preferences.onPreferenceChange(fourStateCookieToggle, newState);
        });
        settingsActivity.finish();
    }

    /**
     * Tests that the Preferences designated by keys in |expectedKeys|, and only
     * these preferences, will be shown for the category specified by |type|. The
     * order of Preferences matters.
     */
    private void checkPreferencesForCategory(
            final @SiteSettingsCategory.Type int type, String[] expectedKeys) {
        final SettingsActivity settingsActivity;

        if (type == SiteSettingsCategory.Type.ALL_SITES
                || type == SiteSettingsCategory.Type.USE_STORAGE) {
            settingsActivity = SiteSettingsTestUtils.startAllSitesSettings(type);
        } else {
            settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(type);
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceFragmentCompat preferenceFragment =
                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
            PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
            int preferenceCount = preferenceScreen.getPreferenceCount();

            ArrayList<String> actualKeys = new ArrayList<>();
            for (int index = 0; index < preferenceCount; index++) {
                Preference preference = preferenceScreen.getPreference(index);
                String key = preference.getKey();
                // Not all Preferences have keys. For example, the list of websites below the
                // toggles, which are dynamically added. Ignore those.
                if (key != null) actualKeys.add(key);
            }

            assertThat(actualKeys,
                    expectedKeys.length == 0 ? emptyIterable() : contains(expectedKeys));
        });
        settingsActivity.finish();
    }

    private void testExpectedPreferences(final @SiteSettingsCategory.Type int type,
            String[] disabledExpectedKeys, String[] enabledExpectedKeys) {
        // Disable the category and check for the right preferences.
        setGlobalToggleForCategory(type, false);
        checkPreferencesForCategory(type, disabledExpectedKeys);
        // Re-enable the category and check for the right preferences.
        setGlobalToggleForCategory(type, true);
        checkPreferencesForCategory(type, enabledExpectedKeys);
    }

    /**
     * Allows cookies to be set and ensures that they are.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesNotBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
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

    /**
     * Clicks on cookies radio buttons and verify the right FPS subpage is launched.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI})
    public void testCookiesFPSSubpageIsLaunched() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);

        verifyFPSCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY);
        verifyFPSCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO);
    }

    private void verifyFPSCookieSubpageIsLaunchedWithParams(
            final SettingsActivity settingsActivity, CookieSettingsState cookieSettingsState) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleCategorySettings websitePreferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            final FourStateCookieSettingsPreference cookies = websitePreferences.findPreference(
                    SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);

            Mockito.clearInvocations(mSettingsLauncher);
            websitePreferences.setSettingsLauncher(mSettingsLauncher);

            SiteSettingsTestUtils.getCookieRadioButtonFrom(cookies, cookieSettingsState)
                    .getAuxButtonForTests()
                    .performClick();

            @CookieControlsMode
            int expectedState = CookieControlsMode.OFF;
            switch (cookieSettingsState) {
                case BLOCK_THIRD_PARTY_INCOGNITO:
                    expectedState = CookieControlsMode.INCOGNITO_ONLY;
                    break;
                case BLOCK_THIRD_PARTY:
                    expectedState = CookieControlsMode.BLOCK_THIRD_PARTY;
                    break;
                default:
                    assert false;
            }

            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putInt(FPSCookieSettings.EXTRA_COOKIE_PAGE_STATE, expectedState);

            Mockito.verify(mSettingsLauncher)
                    .launchSettingsActivity(eq(websitePreferences.getContext()),
                            eq(FPSCookieSettings.class), refEq(fragmentArgs));
        });
    }

    /**
     * Blocks cookies from being set and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(settingsActivity, false);
        settingsActivity.finish();

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks specific sites from setting cookies and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteExceptionCookiesBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(settingsActivity, true);
        settingsActivity.finish();

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
        settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setBlockCookiesSiteException(settingsActivity, url, false);
        settingsActivity.finish();
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks specific sites from setting cookies and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
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

    /**
     * Set a cookie and check that it is removed when a site is cleared.
     */
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

        resetSite(WebsiteAddress.create(url));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set cookies for domains and check that they are removed when a site is cleared.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1329450")
    public void testClearDomainCookies() throws Exception {
        final String url = mPermissionRule.getURLWithHostName(
                "test.example.com", "/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".test.example.com\")");
        Assert.assertEquals("\"Foo=Bar; Foo=Bar\"",
                mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create("test.example.com"));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set the cookie content setting to allow through policy and ensure the correct radio buttons
     * are enabled. This test is executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "1") })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedAllow_EnableHighlight() throws Exception {
        testDefaultCookiesSettingManagedAllowImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "1") })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedAllow_DisableHighlight() throws Exception {
        testDefaultCookiesSettingManagedAllowImpl();
    }

    private void testDefaultCookiesSettingManagedAllowImpl() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to ALLOW) while ThirdPartyCookieBlocking is not
        // managed. This means that every button other than BLOCK is enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.Disabled);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Set the cookie content setting to allow through policy, disable incognito
     * mode and ensure the correct radio buttons are enabled. This test is executed with experiment
     * {@link SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or
     * disabled, and the only expected difference in both cases is the UI that shows the disclaimer
     * that the preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "1") })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedAllowWithIncognitoDisabled_EnableHighlight()
            throws Exception {
        testDefaultCookiesSettingManagedAllowWithIncognitoDisabledImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "1") })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedAllowWithIncognitoDisabled_DisableHighlight()
            throws Exception {
        testDefaultCookiesSettingManagedAllowWithIncognitoDisabledImpl();
    }

    private void testDefaultCookiesSettingManagedAllowWithIncognitoDisabledImpl() throws Exception {
        IncognitoUtils.setEnabledForTesting(false);
        setFourStateCookieToggle(CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO);
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to ALLOW) while ThirdPartyCookieBlocking managed.
        // Cookie toggle is set to block third party incognito but since
        // incognito is disabled the button should be disabled and the allow
        // toggle should be checked.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.Disabled);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Set the cookie content setting to block through a policy and ensure the correct radio buttons
     * are enabled. This test is executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "2") })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedBlock_EnableHighlight() {
        testDefaultCookiesSettingManagedBlockImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "2") })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testDefaultCookiesSettingManagedBlock_DisableHighlight() {
        testDefaultCookiesSettingManagedBlockImpl();
    }

    private void testDefaultCookiesSettingManagedBlockImpl() {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to BLOCK) while ThirdPartyCookieBlocking is not
        // managed. This means cookies should always be blocked, so the user cannot choose any other
        // options and all buttons except the active one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.EnabledChecked);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Enable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled. This test is executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "true") })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testBlockThirdPartyCookiesManagedTrue_EnableHighlight() throws Exception {
        testBlockThirdPartyCookiesManagedTrueImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "true") })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testBlockThirdPartyCookiesManagedTrue_DisableHighlight() throws Exception {
        testBlockThirdPartyCookiesManagedTrueImpl();
    }

    private void testBlockThirdPartyCookiesManagedTrueImpl() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to true) while the ContentSetting is not
        // managed. This means a user can choose only between BLOCK_THIRD_PARTY and BLOCK, so only
        // these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Disable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled. This test is executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "false") })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testBlockThirdPartyCookiesManagedFalse_EnableHighlight() throws Exception {
        testBlockThirdPartyCookiesManagedFalseImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "false") })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testBlockThirdPartyCookiesManagedFalse_DisableHighlight() throws Exception {
        testBlockThirdPartyCookiesManagedFalseImpl();
    }

    private void testBlockThirdPartyCookiesManagedFalseImpl() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to false) while the ContentSetting is not
        // managed. This means a user can only choose to ALLOW all cookies or BLOCK all cookies, so
        // only these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Set both the cookie content setting and third-party cookie blocking through policy and ensure
     * the correct radio buttons are enabled. This test is executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultCookiesSetting", string = "1")
        , @Policies.Item(key = "BlockThirdPartyCookies", string = "false")
    })
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void
    testAllCookieSettingsManaged_EnableHighlight() throws Exception {
        testAllCookieSettingsManagedImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultCookiesSetting", string = "1")
        , @Policies.Item(key = "BlockThirdPartyCookies", string = "false")
    })
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void
    testAllCookieSettingsManaged_DisableHighlight() throws Exception {
        testAllCookieSettingsManagedImpl();
    }

    private void testAllCookieSettingsManagedImpl() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(true);
        // The ContentSetting and ThirdPartyCookieBlocking are managed. This means a user has a
        // fixed setting for cookies that they cannot change. Therefore, all buttons except the
        // selected one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.Disabled);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Ensure no radio buttons are enforced when cookie settings are unmanaged. This test is
     * executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testNoCookieSettingsManaged_EnableHighlight() throws Exception {
        testNoCookieSettingsManagedImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testNoCookieSettingsManaged_DisableHighlight() throws Exception {
        testNoCookieSettingsManagedImpl();
    }

    private void testNoCookieSettingsManagedImpl() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting and ThirdPartyCookieBlocking are unmanaged. This means all buttons
        // should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /**
     * Ensure no radio buttons are enforced when cookie settings are unmanaged. This test is
     * executed with experiment {@link
     * SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID} either enabled or disabled,
     * and the only expected difference in both cases is the UI that shows the disclaimer that the
     * preference is managed.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testNoCookieSettingsManagedWithIncognitoDisabled_EnableHighlight()
            throws Exception {
        testNoCookieSettingsManagedWithIncognitoDisabledImpl();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testNoCookieSettingsManagedWithIncognitoDisabled_DisableHighlight()
            throws Exception {
        testNoCookieSettingsManagedWithIncognitoDisabledImpl();
    }

    private void testNoCookieSettingsManagedWithIncognitoDisabledImpl() throws Exception {
        IncognitoUtils.setEnabledForTesting(false);
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting and ThirdPartyCookieBlocking are unmanaged. This means all buttons
        // should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.ALLOW, ToggleButtonState.EnabledChecked);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, ToggleButtonState.Disabled);
        checkFourStateCookieToggleButtonState(settingsActivity,
                CookieSettingsState.BLOCK_THIRD_PARTY, ToggleButtonState.EnabledUnchecked);
        checkFourStateCookieToggleButtonState(
                settingsActivity, CookieSettingsState.BLOCK, ToggleButtonState.EnabledUnchecked);
        onView(getManagedViewMatcher(/*activeView=*/true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/*activeView=*/false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    private void resetSite(WebsiteAddress address) {
        Website website = new Website(address, address);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();
            websitePreferences.resetSite();
        });
        settingsActivity.finish();
    }

    /**
     * Sets Allow Popups Enabled to be false and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsBlocked() throws TimeoutException {
        new TwoStatePermissionTestCase(
                "Popups", SiteSettingsCategory.Type.POPUPS, ContentSettingsType.POPUPS, false)
                .run();

        // Test that the popup doesn't open.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(1, getTabCount());
    }

    /**
     * Sets Allow Popups Enabled to be true and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsNotBlocked() throws TimeoutException {
        new TwoStatePermissionTestCase(
                "Popups", SiteSettingsCategory.Type.POPUPS, ContentSettingsType.POPUPS, true)
                .run();

        // Test that a popup opens.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(2, getTabCount());
    }

    /**
     * Test that showing the Site Settings menu doesn't crash (crbug.com/610576).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenu() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        settingsActivity.finish();
    }

    /**
     * Test that showing the Site Settings menu contains only the "Cookies" row.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testSiteSettingsMenuWithPSS4Disabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNotNull(websitePreferences.findPreference("cookies"));
        assertNull(websitePreferences.findPreference("third_party_cookies"));
        assertNull(websitePreferences.findPreference("site_data"));
        settingsActivity.finish();
    }

    /**
     * Test that showing the Site Settings menu contains the "Third-party cookies" and "Site data"
     * rows.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testSiteSettingsMenuWithPSS4Enabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNull(websitePreferences.findPreference("cookies"));
        assertNotNull(websitePreferences.findPreference("third_party_cookies"));
        assertNotNull(websitePreferences.findPreference("site_data"));
        settingsActivity.finish();
    }

    /**
     * Tests that only expected Preferences are shown for a category. This
     * santiy checks the number of categories only. Each category has its own
     * individual test below.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesShown() {
        // If you add a category in the SiteSettings UI, please update this total AND add a test for
        // it below, named "testOnlyExpectedPreferences<Category>".
        Assert.assertEquals(28, SiteSettingsCategory.Type.NUM_ENTRIES);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(SiteSettingsFeatureList.SITE_DATA_IMPROVEMENTS)
    public void testOnlyExpectedPreferencesAllSites() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.ALL_SITES, NULL_ARRAY);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(SiteSettingsFeatureList.SITE_DATA_IMPROVEMENTS)
    public void testOnlyExpectedPreferencesAllSitesV2() {
        checkPreferencesForCategory(SiteSettingsCategory.Type.ALL_SITES, CLEAR_BROWSING_DATA_LINK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesADS() {
        testExpectedPreferences(SiteSettingsCategory.Type.ADS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAugmentedReality() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUGMENTED_REALITY, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAutoDarkWebContent() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAutomaticDownloads() {
        testExpectedPreferences(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                BINARY_TOGGLE_WITH_EXCEPTION, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBackgroundSync() {
        testExpectedPreferences(SiteSettingsCategory.Type.BACKGROUND_SYNC,
                BINARY_TOGGLE_WITH_EXCEPTION, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBluetooth() {
        testExpectedPreferences(SiteSettingsCategory.Type.BLUETOOTH, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBluetoothScanning() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesCamera() {
        testExpectedPreferences(SiteSettingsCategory.Type.CAMERA, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesClipboard() {
        testExpectedPreferences(SiteSettingsCategory.Type.CLIPBOARD, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesCookies() {
        String[] cookie = new String[] {"info_text", "four_state_cookie_toggle", "add_exception"};
        setFourStateCookieToggle(CookieSettingsState.ALLOW);
        checkPreferencesForCategory(SiteSettingsCategory.Type.COOKIES, cookie);
        setFourStateCookieToggle(CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO);
        checkPreferencesForCategory(SiteSettingsCategory.Type.COOKIES, cookie);
        setFourStateCookieToggle(CookieSettingsState.BLOCK_THIRD_PARTY);
        checkPreferencesForCategory(SiteSettingsCategory.Type.COOKIES, cookie);
        setFourStateCookieToggle(CookieSettingsState.BLOCK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.COOKIES, cookie);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testOnlyExpectedPreferencesThirdPartyCookies() {
        testExpectedPreferences(SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                new String[] {"info_text", "tri_state_cookie_toggle", "add_exception"},
                new String[] {"info_text", "tri_state_cookie_toggle"});
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testOnlyExpectedPreferencesSiteData() {
        testExpectedPreferences(SiteSettingsCategory.Type.SITE_DATA,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT,
                BINARY_TOGGLE_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testOnlyExpectedExceptionsSiteData() {
        createCookieExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.SITE_DATA);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("secondary.com")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
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
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI)
    public void testExpectedCookieButtonsCheckedWhenFPSUiEnabled() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    preferences.findPreference(SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);

            clickButtonAndVerifyItsChecked(fourStateCookieToggle, CookieSettingsState.ALLOW);
            clickButtonAndVerifyItsChecked(
                    fourStateCookieToggle, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO);
            clickButtonAndVerifyItsChecked(
                    fourStateCookieToggle, CookieSettingsState.BLOCK_THIRD_PARTY);
            clickButtonAndVerifyItsChecked(fourStateCookieToggle, CookieSettingsState.BLOCK);
        });

        settingsActivity.finish();
    }

    private void clickButtonAndVerifyItsChecked(
            FourStateCookieSettingsPreference fourStateCookieToggle, CookieSettingsState state) {
        fourStateCookieToggle.getButton(state).performClick();
        Assert.assertTrue(
                "Button should be checked.", fourStateCookieToggle.getButton(state).isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI,
            ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4})
    public void
    testExpectedCookieButtonsCheckedWhenFPSUiAndPSS4Enabled() {
        SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            TriStateCookieSettingsPreference threeStateCookieToggle =
                    preferences.findPreference(SingleCategorySettings.TRI_STATE_COOKIE_TOGGLE);

            clickButtonAndVerifyItsChecked(threeStateCookieToggle, CookieControlsMode.OFF);
            clickButtonAndVerifyItsChecked(
                    threeStateCookieToggle, CookieControlsMode.INCOGNITO_ONLY);
            clickButtonAndVerifyItsChecked(
                    threeStateCookieToggle, CookieControlsMode.BLOCK_THIRD_PARTY);
        });

        settingsActivity.finish();
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
    public void testOnlyExpectedPreferencesDeviceLocation() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        testExpectedPreferences(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE, BINARY_TOGGLE);

        // Disable system location setting and check for the right preferences.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION, BINARY_TOGGLE_WITH_OS_WARNING_EXTRA);
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesFederatedIdentityAPI() {
        testExpectedPreferences(SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                BINARY_TOGGLE_WITH_EXCEPTION, BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesIdleDetection() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.IDLE_DETECTION, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesJavascript() {
        testExpectedPreferences(SiteSettingsCategory.Type.JAVASCRIPT, BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesMicrophone() {
        testExpectedPreferences(SiteSettingsCategory.Type.MICROPHONE, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesNFC() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);

        testExpectedPreferences(SiteSettingsCategory.Type.NFC, BINARY_TOGGLE, BINARY_TOGGLE);

        // Disable system nfc setting and check for the right preferences.
        NfcSystemLevelSetting.setNfcSettingForTesting(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC, BINARY_TOGGLE_WITH_OS_WARNING_EXTRA);
        NfcSystemLevelSetting.setNfcSettingForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("QuietNotificationPrompts")
    public void testOnlyExpectedPreferencesNotifications() {
        String[] notifications_enabled;
        String[] notifications_disabled;
        // The "notifications_vibrate" option has been removed in Android O but is present in
        // earlier versions.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            notifications_enabled = new String[] {
                    "binary_toggle", "notifications_quiet_ui", "notifications_vibrate"};
            notifications_disabled = new String[] {"binary_toggle", "notifications_vibrate"};
        } else {
            notifications_enabled = new String[] {"binary_toggle", "notifications_quiet_ui"};
            notifications_disabled = BINARY_TOGGLE;
        }

        testExpectedPreferences(SiteSettingsCategory.Type.NOTIFICATIONS, notifications_disabled,
                notifications_enabled);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesPopups() {
        testExpectedPreferences(SiteSettingsCategory.Type.POPUPS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesProtectedMedia() {
        String[] protectedMedia = new String[] {"tri_state_toggle", "protected_content_learn_more"};
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ALLOW);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ASK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.BLOCK);
        checkPreferencesForCategory(SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testOnlyExpectedPreferencesRequestDesktopSite() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, BINARY_TOGGLE, BINARY_TOGGLE);
        Assert.assertTrue(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be updated.",
                ContextUtils.getAppSharedPreferences().contains(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testOnlyExpectedPreferencesRequestDesktopSiteDomainSettings() {
        testExpectedPreferences(SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                BINARY_TOGGLE_WITH_EXCEPTION, BINARY_TOGGLE_WITH_EXCEPTION);
        Assert.assertTrue(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be updated.",
                ContextUtils.getAppSharedPreferences().contains(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_ADDITIONS)
    @DisableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testOnlyExpectedPreferencesRequestDesktopSiteAdditionalSettings() {
        String[] rdsDisabled = {"binary_toggle", "desktop_site_peripheral", "desktop_site_display"};
        testExpectedPreferences(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, rdsDisabled, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesSensors() {
        testExpectedPreferences(SiteSettingsCategory.Type.SENSORS, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesSound() {
        testExpectedPreferences(SiteSettingsCategory.Type.SOUND, BINARY_TOGGLE_WITH_EXCEPTION,
                BINARY_TOGGLE_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesUSB() {
        testExpectedPreferences(SiteSettingsCategory.Type.USB, BINARY_TOGGLE, BINARY_TOGGLE);
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
    public void testOnlyExpectedPreferencesVirtualReality() {
        testExpectedPreferences(
                SiteSettingsCategory.Type.VIRTUAL_REALITY, BINARY_TOGGLE, BINARY_TOGGLE);
    }

    /**
     * Tests system NFC support in Preferences.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSystemNfcSupport() {
        // Disable system nfc support and check for the right preferences.
        NfcSystemLevelSetting.setNfcSupportForTesting(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC, BINARY_TOGGLE_WITH_OS_WARNING_EXTRA);
    }

    /**
     * Tests that {@link SingleWebsiteSettings#resetSite} doesn't crash
     * (see e.g. the crash on host names in issue 600232).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDoesntCrash() {
        WebsiteAddress address = WebsiteAddress.create("example.com");
        resetSite(address);
    }

    /**
     * Sets Allow Camera Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCameraBlocked() throws Exception {
        new TwoStatePermissionTestCase("Camera", SiteSettingsCategory.Type.CAMERA,
                ContentSettingsType.MEDIASTREAM_CAMERA, false)
                .run();

        // Test that the camera permission doesn't get requested.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: true, audio: false});", 0, true /* withGesture */,
                true /* isDialog */);
    }

    /**
     * Sets Allow Camera Enabled to be true and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testCameraNotBlocked() throws Exception {
        new TwoStatePermissionTestCase("Camera", SiteSettingsCategory.Type.CAMERA,
                ContentSettingsType.MEDIASTREAM_CAMERA, true)
                .run();

        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: true, audio: false});", 0, true /* withGesture */,
                true /* isDialog */);
    }

    /**
     * Sets Allow Mic Enabled to be false and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicBlocked() throws Exception {
        new TwoStatePermissionTestCase("Mic", SiteSettingsCategory.Type.MICROPHONE,
                ContentSettingsType.MEDIASTREAM_MIC, false)
                .run();

        // Test that the microphone permission doesn't get requested.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: false, audio: true});", 0, true, true);
    }

    /**
     * Sets Allow Mic Enabled to be true and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicNotBlocked() throws Exception {
        new TwoStatePermissionTestCase("Mic", SiteSettingsCategory.Type.MICROPHONE,
                ContentSettingsType.MEDIASTREAM_MIC, true)
                .run();

        // Launch a page that uses the microphone and make sure a permission prompt shows up.
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: false, audio: true});", 0, true, true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBackgroundSync() {
        new TwoStatePermissionTestCase("BackgroundSync", SiteSettingsCategory.Type.BACKGROUND_SYNC,
                ContentSettingsType.BACKGROUND_SYNC, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBackgroundSync() {
        new TwoStatePermissionTestCase("BackgroundSync", SiteSettingsCategory.Type.BACKGROUND_SYNC,
                ContentSettingsType.BACKGROUND_SYNC, false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowUsb() {
        new TwoStatePermissionTestCase(
                "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockUsb() {
        new TwoStatePermissionTestCase(
                "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAutomaticDownloads() {
        new TwoStatePermissionTestCase("AutomaticDownloads",
                SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                ContentSettingsType.AUTOMATIC_DOWNLOADS, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAutomaticDownloads() {
        new TwoStatePermissionTestCase("AutomaticDownloads",
                SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                ContentSettingsType.AUTOMATIC_DOWNLOADS, false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothScanning() {
        new TwoStatePermissionTestCase("BluetoothScanning",
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                ContentSettingsType.BLUETOOTH_SCANNING, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothScanning() {
        new TwoStatePermissionTestCase("BluetoothScanning",
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                ContentSettingsType.BLUETOOTH_SCANNING, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothGuard() {
        new TwoStatePermissionTestCase("BluetoothGuard", SiteSettingsCategory.Type.BLUETOOTH,
                ContentSettingsType.BLUETOOTH_GUARD, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothGuard() {
        new TwoStatePermissionTestCase("BluetoothGuard", SiteSettingsCategory.Type.BLUETOOTH,
                ContentSettingsType.BLUETOOTH_GUARD, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new TwoStatePermissionTestCase(
                "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new TwoStatePermissionTestCase(
                "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAr() {
        new TwoStatePermissionTestCase(
                "AR", SiteSettingsCategory.Type.AUGMENTED_REALITY, ContentSettingsType.AR, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAr() {
        new TwoStatePermissionTestCase(
                "AR", SiteSettingsCategory.Type.AUGMENTED_REALITY, ContentSettingsType.AR, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowVr() {
        new TwoStatePermissionTestCase(
                "VR", SiteSettingsCategory.Type.VIRTUAL_REALITY, ContentSettingsType.VR, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockVr() {
        new TwoStatePermissionTestCase(
                "VR", SiteSettingsCategory.Type.VIRTUAL_REALITY, ContentSettingsType.VR, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowIdleDetection() {
        new TwoStatePermissionTestCase("IdleDetection", SiteSettingsCategory.Type.IDLE_DETECTION,
                ContentSettingsType.IDLE_DETECTION, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockIdleDetection() {
        new TwoStatePermissionTestCase("IdleDetection", SiteSettingsCategory.Type.IDLE_DETECTION,
                ContentSettingsType.IDLE_DETECTION, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Enabled";
        final int preTestCount = RecordHistogram.getHistogramValueCountForTesting(
                histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCase("AutoDarkWebContent",
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                ContentSettingsType.AUTO_DARK_WEB_CONTENT, true)
                .run();
        Assert.assertEquals("<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Disabled";
        final int preTestCount = RecordHistogram.getHistogramValueCountForTesting(
                histogramName, SITE_SETTINGS_GLOBAL);
        new TwoStatePermissionTestCase("AutoDarkWebContent",
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                ContentSettingsType.AUTO_DARK_WEB_CONTENT, false)
                .run();
        Assert.assertEquals("<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testAllowRequestDesktopSite() {
        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testBlockRequestDesktopSite() {
        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testAllowRequestDesktopSiteDomainSetting() {
        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, true)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowRequestDesktopSiteDomainSetting_DowngradePath() {
        // Enable RDS exceptions.
        Map<String, Boolean> featureMap = new HashMap<>();
        featureMap.put(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, true);
        FeatureList.setTestFeatures(featureMap);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(
                    getBrowserContextHandle(), ContentSettingsType.REQUEST_DESKTOP_SITE,
                    "https://example.com", "https://example.com", ContentSettingValues.ALLOW);
        });

        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, true)
                .withExpectedPrefKeys("allowed_group")
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();

        // Disable RDS exceptions for a downgrade.
        featureMap.put(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, false);
        featureMap.put(SiteSettingsFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS_DOWNGRADE, true);
        FeatureList.setTestFeatures(featureMap);

        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)
    public void testBlockRequestDesktopSiteDomainSetting() {
        new TwoStatePermissionTestCase("RequestDesktopSite",
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingsType.REQUEST_DESKTOP_SITE, false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowFederatedIdentityApi() {
        new TwoStatePermissionTestCase("FederatedIdentityApi",
                SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                ContentSettingsType.FEDERATED_IDENTITY_API, true)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockFederatedIdentityApi() {
        new TwoStatePermissionTestCase("FederatedIdentityApi",
                SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                ContentSettingsType.FEDERATED_IDENTITY_API, false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.
    Build(message = "Flaky, see crbug.com/1170671", sdk_is_less_than = Build.VERSION_CODES.Q)
    public void testEmbargoedNotificationSiteSettings() throws Exception {
        final String url = mPermissionRule.getURLWithHostName(
                "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(url);

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = settingsLauncher.createSettingsActivityIntent(context,
                SingleWebsiteSettings.class.getName(),
                SingleWebsiteSettings.createFragmentArgsForSite(url));
        final SettingsActivity settingsActivity =
                (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();

            final Preference notificationPreference =
                    websitePreferences.findPreference("push_notifications_list");

            Assert.assertEquals(context.getString(R.string.automatically_blocked),
                    notificationPreference.getSummary());

            websitePreferences.launchOsChannelSettingsFromPreference(notificationPreference);

            // Ensure that a proper separate channel has indeed been created to allow the user to
            // alter the setting.
            Assert.assertNotEquals(ChromeChannelDefinitions.ChannelId.SITES,
                    SiteChannelsManager.getInstance().getChannelIdForOrigin(
                            Origin.createOrThrow(url).toString()));
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
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        final String urlToEmbargo = mPermissionRule.getURLWithHostName(
                "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(urlToEmbargo);

        final String urlToBlock = mPermissionRule.getURLWithHostName(
                "exampleToBlock.com", "/chrome/test/data/notifications/notification_tester.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(
                    getBrowserContextHandle(), ContentSettingsType.NOTIFICATIONS, urlToBlock,
                    urlToBlock, ContentSettingValues.BLOCK);
        });

        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.NOTIFICATIONS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean blockedByEmbargo =
                    WebsitePreferenceBridgeJni.get().isNotificationEmbargoedForOrigin(
                            getBrowserContextHandle(), urlToEmbargo);
            Assert.assertTrue(blockedByEmbargo);

            final String blockedGroupKey = "blocked_group";
            // Click on Blocked group in Category Settings. By default Blocked is closed, to be able
            // to find any origins inside, Blocked should be opened.
            SingleCategorySettings websitePreferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            websitePreferences.findPreference(blockedGroupKey).performClick();

            // After triggering onClick on Blocked group, all UI will be discarded and reinitialized
            // from scratch. Init all variables again, otherwise it will use stale information.
            websitePreferences = (SingleCategorySettings) settingsActivity.getMainFragment();
            ExpandablePreferenceGroup blockedGroup =
                    (ExpandablePreferenceGroup) websitePreferences.findPreference(blockedGroupKey);

            Assert.assertTrue(blockedGroup.isExpanded());
            // Only |url| has been added under embargo.
            Assert.assertEquals(2, blockedGroup.getPreferenceCount());

            Assert.assertEquals(InstrumentationRegistry.getTargetContext().getString(
                                        R.string.automatically_blocked),
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
        final String rpUrl = mPermissionRule.getURLWithHostName(
                "example.com", "/chrome/test/data/android/simple.html");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FederatedIdentityTestUtils.embargoFedCmForRelyingParty(new GURL(rpUrl)); });

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = settingsLauncher.createSettingsActivityIntent(context,
                SingleWebsiteSettings.class.getName(),
                SingleWebsiteSettings.createFragmentArgsForSite(rpUrl));
        final SettingsActivity settingsActivity =
                (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();
            final Preference fedCmPreference =
                    websitePreferences.findPreference("federated_identity_api_list");

            Assert.assertEquals(context.getString(R.string.automatically_blocked),
                    fedCmPreference.getSummary());
        });
        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentDefaultOption() throws Exception {
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskAllow() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ASK);

        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ASK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runDenyTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.BLOCK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "https://crbug.com/1269556",
        sdk_is_greater_than = Build.VERSION_CODES.P)
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1234530
    public void testProtectedContentAllowThenBlock() throws Exception {
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);

        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.BLOCK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_ADDITIONS)
    public void testDesktopSitePeripherals() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            ChromeBaseCheckBoxPreference peripheralPref = preferences.findPreference(
                    SingleCategorySettings.DESKTOP_SITE_PERIPHERAL_TOGGLE_KEY);
            PrefService prefService = UserPrefs.get(getBrowserContextHandle());
            Assert.assertFalse("Peripherals setting should be OFF.",
                    prefService.getBoolean(DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED));

            preferences.onPreferenceChange(peripheralPref, true);
            Assert.assertTrue("Peripherals setting should be ON.",
                    prefService.getBoolean(DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED));

            preferences.onPreferenceChange(peripheralPref, false);
            Assert.assertFalse("Peripherals setting should be OFF.",
                    prefService.getBoolean(DESKTOP_SITE_PERIPHERAL_SETTING_ENABLED));
        });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_ADDITIONS)
    public void testDesktopSiteExternalDisplay() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            ChromeBaseCheckBoxPreference externalDisplayPref = preferences.findPreference(
                    SingleCategorySettings.DESKTOP_SITE_DISPLAY_TOGGLE_KEY);
            PrefService prefService = UserPrefs.get(getBrowserContextHandle());
            Assert.assertFalse("Display setting should be OFF.",
                    prefService.getBoolean(DESKTOP_SITE_DISPLAY_SETTING_ENABLED));

            preferences.onPreferenceChange(externalDisplayPref, true);
            Assert.assertTrue("Display setting should be ON.",
                    prefService.getBoolean(DESKTOP_SITE_DISPLAY_SETTING_ENABLED));

            preferences.onPreferenceChange(externalDisplayPref, false);
            Assert.assertFalse("Display setting should be OFF.",
                    prefService.getBoolean(DESKTOP_SITE_DISPLAY_SETTING_ENABLED));
        });
        settingsActivity.finish();
    }

    private void renderCategoryPage(@SiteSettingsCategory.Type int category, String name)
            throws IOException {
        createCookieExceptions();
        var settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(category);
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, name);
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testRenderSiteDataPage() throws Exception {
        renderCategoryPage(SiteSettingsCategory.Type.SITE_DATA, "site_settings_site_data_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI)
    public void testRenderThirdPartyCookiesPage() throws Exception {
        renderCategoryPage(SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4,
            ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI})
    public void
    testRenderThirdPartyCookiesPageWithFPS() throws Exception {
        renderCategoryPage(SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                "site_settings_third_party_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4,
            ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI})
    public void
    testRenderCookiesPage() throws Exception {
        renderCategoryPage(SiteSettingsCategory.Type.COOKIES, "site_settings_cookies_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI)
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testRenderCookiesPageWithFPS() throws Exception {
        renderCategoryPage(SiteSettingsCategory.Type.COOKIES, "site_settings_cookies_page_fps");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderLocationPage() throws Exception {
        renderCategoryPage(
                SiteSettingsCategory.Type.DEVICE_LOCATION, "site_settings_location_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderProtectedMediaPage() throws Exception {
        renderCategoryPage(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, "site_settings_protected_media_page");
    }

    /** Test case for checking that settings with binary toggles are disabled by policy.*/
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultJavaScriptSetting", string = "2")
        , @Policies.Item(key = "DefaultPopupsSetting", string = "2"),
                @Policies.Item(key = "DefaultGeolocationSetting", string = "2")
    })
    public void
    testAllTwoStateToggleDisabledByPolicy() {
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.POPUPS);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.DEVICE_LOCATION);
        // TODO(crbug/1385889): add a test for sensors once crash in the sensors settings page is
        // resolved.
    }

    public void testTwoStateToggleDisabledByPolicy(@SiteSettingsCategory.Type int type) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);
        SingleCategorySettings singleCategorySettings =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        ChromeSwitchPreference binaryToggle =
                (ChromeSwitchPreference) singleCategorySettings.findPreference(
                        SingleCategorySettings.BINARY_TOGGLE_KEY);

        Assert.assertFalse(binaryToggle.isEnabled());
    }

    /**
     * Allows third party cookies for a website, and tests that the UI shows a managed preference
     * in the allowed group. Checks that it shows the toast when the preference is clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.
    Add({ @Policies.Item(key = "CookiesAllowedForUrls", string = "[\"[*.]chromium.org\"]") })
    public void testAllowCookiesForURL_DisableHighlightManagedPrefDisclaimerAndroid()
            throws Exception {
        testCookiesSettingsManagedForURL(SingleCategorySettings.ALLOWED_GROUP);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.
    Add({ @Policies.Item(key = "CookiesAllowedForUrls", string = "[\"[*.]chromium.org\"]") })
    public void testAllowCookiesForURL_EnableHighlightManagedPrefDisclaimerAndroid()
            throws Exception {
        testCookiesSettingsManagedForURL(SingleCategorySettings.ALLOWED_GROUP);
    }

    /**
     * Blocks third party cookies for a website, and tests that the UI shows a managed preference
     * in the blocked group. Checks that it shows toast when the preference is clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.
    Add({ @Policies.Item(key = "CookiesBlockedForUrls", string = "[\"[*.]chromium.org\"]") })
    public void testBlockCookiesForURL_DisableHighlightManagedPrefDisclaimerAndroid()
            throws Exception {
        testCookiesSettingsManagedForURL(SingleCategorySettings.BLOCKED_GROUP);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.
    Add({ @Policies.Item(key = "CookiesBlockedForUrls", string = "[\"[*.]chromium.org\"]") })
    public void testBlockCookiesForURL_EnableHighlightManagedPrefDisclaimerAndroid()
            throws Exception {
        testCookiesSettingsManagedForURL(SingleCategorySettings.BLOCKED_GROUP);
    }

    public void testCookiesSettingsManagedForURL(String setting) throws Exception {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        String managedText = InstrumentationRegistry.getTargetContext().getString(
                R.string.managed_by_your_organization);

        SingleCategorySettings websitePreferences =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        ExpandablePreferenceGroup managedGroup =
                (ExpandablePreferenceGroup) websitePreferences.findPreference(setting);
        Assert.assertTrue("The blocked group should be expanded.", managedGroup.isExpanded());
        Assert.assertEquals("The blocked expandable group should have exactly one website listed.",
                1, managedGroup.getPreferenceCount());
        ChromeImageViewPreference websitePreference =
                (ChromeImageViewPreference) managedGroup.getPreference(0);

        /*
         * Swipes to the end of the screen to show the website preference for the blocked site
         * then checks that the content description and the summary text reflect the managed state.
         */
        onView(ViewMatchers.isRoot()).perform(swipeUp());
        onData(withKey(setting))
                .inAdapterView(allOf(withContentDescription(R.string.managed_by_your_organization),
                        withText(R.string.managed_by_your_organization), isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> { websitePreference.performClick(); });
        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(allOf(withId(R.id.toast_text))))
                .check(matches(isDisplayed()));
    }

    static class PermissionTestCase {
        protected final String mTestName;
        protected final @SiteSettingsCategory.Type int mSiteSettingsType;
        protected final @ContentSettingsType int mContentSettingsType;
        protected final boolean mIsCategoryEnabled;
        protected final List<String> mExpectedPreferenceKeys;

        protected SettingsActivity mSettingsActivity;

        PermissionTestCase(final String testName,
                @SiteSettingsCategory.Type final int siteSettingsType,
                @ContentSettingsType final int contentSettingsType, final boolean enabled) {
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

        public void run() {
            mSettingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(mSiteSettingsType);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
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

            Assert.assertEquals(actualKeys.toString() + " should match " + expectedKeys.toString(),
                    expectedKeys, actualKeys);
        }
    }

    /** Test case for site settings with a global toggle.  */
    static class TwoStatePermissionTestCase extends PermissionTestCase {
        TwoStatePermissionTestCase(
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
            final String exceptionString = "Test <" + mTestName + ">: Content setting category <"
                    + mContentSettingsType + "> should be "
                    + (mIsCategoryEnabled ? "enabled" : "disabled") + " with Site Settings <"
                    + mSiteSettingsType + ">.";

            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            assertNotNull("Toggle should not be null.", toggle);

            singleCategorySettings.onPreferenceChange(toggle, mIsCategoryEnabled);
            Assert.assertEquals(exceptionString, mIsCategoryEnabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), mContentSettingsType));
        }

        /** Verfiy {@link ContentSettingsResources} is set correctly. */
        private void assertToggleTitleAndSummary(SingleCategorySettings singleCategorySettings) {
            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            assert toggle != null;

            var delegate = new ChromeSiteSettingsDelegate(
                    toggle.getContext(), Profile.getLastUsedRegularProfile());

            Assert.assertEquals("Preference title is not set correctly.",
                    singleCategorySettings.getResources().getString(
                            ContentSettingsResources.getTitle(mContentSettingsType, delegate)),
                    toggle.getTitle());
            assertNotNull("Enabled summary text should not be null.", toggle.getSummaryOn());
            assertNotNull("Disabled summary text should not be null.", toggle.getSummaryOff());

            String summary = mIsCategoryEnabled ? toggle.getSummaryOn().toString()
                                                : toggle.getSummaryOff().toString();
            String expected = singleCategorySettings.getResources().getString(mIsCategoryEnabled
                            ? ContentSettingsResources.getEnabledSummary(
                                    mContentSettingsType, delegate)
                            : ContentSettingsResources.getDisabledSummary(
                                    mContentSettingsType, delegate));
            Assert.assertEquals(
                    "Summary text in state <" + mIsCategoryEnabled + "> does not match.", expected,
                    summary);
        }
    }
}
