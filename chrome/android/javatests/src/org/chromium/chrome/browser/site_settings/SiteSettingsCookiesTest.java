// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.preference.Preference;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.CookieSettings;
import org.chromium.components.browser_ui.site_settings.CookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.GroupedWebsitesSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsiteGroup;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for cookie settings and storage access in Site Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DisableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
public class SiteSettingsCookiesTest {

    private static final String PRIMARY_PATTERN_WITH_WILDCARD = "http://[*.]primary.com";

    private static final String SECONDARY_PATTERN_WITH_WILDCARD = "http://[*.]secondary.com";

    private static void createCookieExceptionsWithWildcards() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            SiteSettingsTestHelper.getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "*",
                            SECONDARY_PATTERN_WITH_WILDCARD,
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            SiteSettingsTestHelper.getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            PRIMARY_PATTERN_WITH_WILDCARD,
                            "*",
                            ContentSetting.ALLOW);
                });
    }

    private static Matcher<View> getManagedViewMatcher(boolean activeView) {
        return activeView
                ? allOf(
                        withId(R.id.managed_disclaimer_text),
                        hasSibling(withId(R.id.radio_button_layout)))
                : withId(R.id.managed_view_legacy);
    }

    private static void resetGroup(List<WebsiteAddress> addresses) {
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
        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    public PermissionTestRule mPermissionTestRule =
            new PermissionTestRule(mActivityTestRule.getActivityTestRule(), true);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    public AdvancedProtectionTestRule mAdvancedProtectionRule = new AdvancedProtectionTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAdvancedProtectionRule)
                    .around(mActivityTestRule)
                    .around(mPermissionTestRule);

    @Mock private SettingsNavigation mSettingsNavigation;

    @Before
    public void setUp() throws TimeoutException {
        try {
            SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
        } catch (TimeoutException e) {
            // Sometimes there's a callback timeout here. Doesn't seem to impact test results.
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.INCOGNITO_ONLY);
                });
    }

    @After
    public void tearDown() throws Exception {
        SiteSettingsTestHelper.cleanUpContentSettingsAndExceptions(new CallbackHelper());
    }

    private void setCookiesEnabled(final SettingsActivity settingsActivity, final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        final SingleCategorySettings websitePreferences =
                                (SingleCategorySettings) settingsActivity.getMainFragment();
                        final CookieSettingsPreference cookies =
                                websitePreferences.findPreference(
                                        SingleCategorySettings.COOKIE_TOGGLE);

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

    @IntDef({
        ToggleButtonState.ENABLED_UNCHECKED,
        ToggleButtonState.ENABLED_CHECKED,
        ToggleButtonState.DISABLED
    })
    private @interface ToggleButtonState {
        int ENABLED_UNCHECKED = 0;
        int ENABLED_CHECKED = 1;
        int DISABLED = 2;
    }

    private void checkCookieToggleButtonState(
            final SettingsActivity settingsActivity,
            final @CookieControlsMode int state,
            final @ToggleButtonState int toggleState) {
        waitForCookieToggleToBeBound(settingsActivity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    CookieSettingsPreference cookieToggle =
                            preferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);
                    boolean enabled = toggleState != ToggleButtonState.DISABLED;
                    boolean checked = toggleState == ToggleButtonState.ENABLED_CHECKED;
                    Assert.assertEquals(
                            state + " button should be " + (enabled ? "enabled" : "disabled"),
                            enabled,
                            cookieToggle.isButtonEnabledForTesting(state));
                    Assert.assertEquals(
                            state + " button should be " + (checked ? "checked" : "unchecked"),
                            checked,
                            cookieToggle.isButtonCheckedForTesting(state));
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
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.COOKIES));
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

    private ViewInteraction getImageViewWidget(String preferenceTitle) {
        return onView(
                allOf(
                        withId(R.id.image_view_widget),
                        isDescendantOfA(withChild(withChild(withText(preferenceTitle))))));
    }

    private CookieSettingsPreference getCookieToggle(SettingsActivity settingsActivity) {
        SingleCategorySettings preferences =
                (SingleCategorySettings) settingsActivity.getMainFragment();
        return preferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);
    }

    private CookieSettingsPreference waitForCookieToggleToBeBound(
            SettingsActivity settingsActivity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    CookieSettingsPreference preference = getCookieToggle(settingsActivity);
                    return preference != null
                            && preference.getButton(CookieControlsMode.BLOCK_THIRD_PARTY) != null;
                },
                "Cookie toggle button was never bound to the view.");
        return getCookieToggle(settingsActivity);
    }

    private void clickButtonAndVerifyItsChecked(
            CookieSettingsPreference cookieToggle, @CookieControlsMode int state) {
        cookieToggle.getButton(state).performClick();
        Assert.assertTrue("Button should be checked.", cookieToggle.getButton(state).isChecked());
    }

    private void verifyFpsCookieSubpageIsLaunchedWithParams(
            final SettingsActivity settingsActivity,
            @CookieControlsMode int expectedCookieControlMode) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleCategorySettings websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    final CookieSettingsPreference cookies =
                            websitePreferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);

                    Mockito.clearInvocations(mSettingsNavigation);
                    websitePreferences.setSettingsNavigation(mSettingsNavigation);

                    SiteSettingsTestUtils.getCookieRadioButtonFrom(
                                    cookies, expectedCookieControlMode)
                            .getAuxButtonForTests()
                            .performClick();

                    Bundle fragmentArgs = new Bundle();
                    fragmentArgs.putInt(
                            CookieSettings.EXTRA_COOKIE_PAGE_STATE, expectedCookieControlMode);

                    Mockito.verify(mSettingsNavigation)
                            .startSettings(
                                    eq(websitePreferences.getContext()),
                                    eq(CookieSettings.class),
                                    refEq(fragmentArgs),
                                    eq(true));
                });
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

        final String url = mPermissionTestRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionTestRule.loadUrl(url);
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie still is set.
        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /** Clicks on cookies radio buttons and verify the right FPS subpage is launched. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testCookiesFpsSubpageIsLaunched() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        waitForCookieToggleToBeBound(settingsActivity);

        verifyFpsCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY);
        verifyFpsCookieSubpageIsLaunchedWithParams(
                settingsActivity, CookieControlsMode.INCOGNITO_ONLY);
    }

    /** Blocks specific sites from setting cookies and ensures that no cookies can be set. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/40881976")
    public void testSiteExceptionSiteDataBlocked() throws Exception {
        SiteSettingsTestHelper.setGlobalToggleForCategory(
                SiteSettingsCategory.Type.SITE_DATA, true);

        final String url = mPermissionTestRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionTestRule.loadUrl(url);
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Check cookies can be set for this website when there is no rule.
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Set specific rule to block site and ensure it cannot set cookies.
        mPermissionTestRule.loadUrl(url);
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("clearCookie()");

        SiteSettingsTestHelper.setGlobalToggleForCategory(
                SiteSettingsCategory.Type.SITE_DATA, false);
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /** Set a cookie and check that it is removed when a site is cleared. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/40709705")
    public void testClearCookies() throws Exception {
        final String url = mPermissionTestRule.getURL("/chrome/test/data/android/cookie.html");

        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);

        SiteSettingsTestHelper.resetSite(WebsiteAddress.create(url));

        // Load the page again and ensure the cookie is gone.
        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }

    /** Tests clearing cookies for a group of websites. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testClearCookiesGroup() throws Exception {
        final String url1 =
                mPermissionTestRule.getURLWithHostName(
                        "one.example.com", "/chrome/test/data/android/cookie.html");
        final String url2 =
                mPermissionTestRule.getURLWithHostName(
                        "two.example.com", "/chrome/test/data/android/cookie.html");
        final String url3 =
                mPermissionTestRule.getURLWithHostName(
                        "foo.com", "/chrome/test/data/android/cookie.html");

        mPermissionTestRule.loadUrl(url1);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".one.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        mPermissionTestRule.loadUrl(url2);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".two.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        mPermissionTestRule.loadUrl(url3);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".foo.com\")");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);

        resetGroup(Arrays.asList(WebsiteAddress.create(url1), WebsiteAddress.create(url2)));

        // 1 and 2 got cleared; 3 stays intact.
        mPermissionTestRule.loadUrl(url1);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.loadUrl(url2);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.loadUrl(url3);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }

    /** Set cookies for domains and check that they are removed when a site is cleared. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/40842614")
    public void testClearDomainCookies() throws Exception {
        final String url =
                mPermissionTestRule.getURLWithHostName(
                        "test.example.com", "/chrome/test/data/android/cookie.html");

        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".test.example.com\")");
        Assert.assertEquals(
                "\"Foo=Bar; Foo=Bar\"",
                mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        SiteSettingsTestHelper.resetSite(WebsiteAddress.create("test.example.com"));

        // Load the page again and ensure the cookie is gone.
        mPermissionTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"\"", mPermissionTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set the cookie content setting to allow through policy and ensure the correct radio buttons
     * are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({@Policies.Item(key = "DefaultCookiesSetting", string = "1")})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testDefaultCookiesSettingManagedAllow() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to ALLOW) while ThirdPartyCookieBlocking is not
        // managed. This means that every button other than BLOCK is enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.ENABLED_CHECKED);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.ENABLED_UNCHECKED);
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testBlockThirdPartyCookiesManagedTrue() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to true) while the ContentSetting is not
        // managed. This means a user can choose only between BLOCK_THIRD_PARTY and BLOCK, so only
        // these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity, CookieControlsMode.INCOGNITO_ONLY, ToggleButtonState.DISABLED);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.ENABLED_CHECKED);
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // crbug.com/481297705
    public void testBlockThirdPartyCookiesManagedFalse() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to false) while the ContentSetting is not
        // managed. This means a user can only choose to ALLOW all cookies or BLOCK all cookies, so
        // only these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.ENABLED_CHECKED);
        checkCookieToggleButtonState(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY, ToggleButtonState.DISABLED);
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testAllCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(true);
        // The ContentSetting and ThirdPartyCookieBlocking are managed. This means a user has a
        // fixed setting for cookies that they cannot change. Therefore, all buttons except the
        // selected one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.ENABLED_CHECKED);
        checkCookieToggleButtonState(
                settingsActivity, CookieControlsMode.BLOCK_THIRD_PARTY, ToggleButtonState.DISABLED);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(isDisplayed()));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /** Ensure no radio buttons are enforced when cookie settings are unmanaged. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testNoCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting and ThirdPartyCookieBlocking are unmanaged. This means all buttons
        // should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.ENABLED_CHECKED);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.ENABLED_UNCHECKED);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    /** Ensure correct radio buttons are shown. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void blockAndAllowThirdPartyCookieOptionsShown() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.INCOGNITO_ONLY,
                ToggleButtonState.ENABLED_CHECKED);
        checkCookieToggleButtonState(
                settingsActivity,
                CookieControlsMode.BLOCK_THIRD_PARTY,
                ToggleButtonState.ENABLED_UNCHECKED);
        onView(getManagedViewMatcher(/* activeView= */ true)).check(matches(not(isDisplayed())));
        onView(getManagedViewMatcher(/* activeView= */ false)).check(matches(not(isDisplayed())));
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesThirdPartyCookies() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                new String[] {"info_text", "cookie_toggle", "add_exception"},
                new String[] {"info_text", "cookie_toggle", "add_exception"});
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesSiteData() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.SITE_DATA,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedExceptionsSiteData() {
        SiteSettingsTestHelper.createCookieExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.SITE_DATA);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("secondary.com")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedExceptionsThirdPartyCookies() {
        SiteSettingsTestHelper.createCookieExceptions();
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
    public void testOnlyExpectedPreferencesStorageAccess() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.STORAGE_ACCESS,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    @DisabledTest(message = "https://crbug.com/433576895")
    public void testExpectedExceptionsStorageAccess() {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.STORAGE_ACCESS);

        onView(withText("primary.com")).check(matches(isDisplayed()));
        onView(withText("2 sites")).check(matches(isDisplayed()));
        onView(withText("primary2.com")).check(matches(isDisplayed()));
        onView(withText("1 site")).check(matches(isDisplayed()));

        getImageViewWidget("primary.com").check(matches(isDisplayed())).perform(click());

        // Check that the subpage is shown with the correct origins.
        onView(withText("primary.com")).check(matches(isDisplayed()));
        waitForView(withText("secondary.com"));
        waitForView(withText("secondary3.com"));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/433576895")
    public void testResetExceptionGroupStorageAccess() {
        SiteSettingsTestHelper.createStorageAccessExceptions();
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
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary.com")));
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary2.com"),
                                    new GURL("https://secondary2.com")));
                });

        onView(withText("primary.com")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/433576895")
    public void testBlockExceptionGroupStorageAccess() {
        SiteSettingsTestHelper.createStorageAccessExceptions();
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
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary.com")));
                    assertEquals(
                            ContentSetting.BLOCK,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary2.com"),
                                    new GURL("https://secondary2.com")));
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testStorageAccessSubpage() {
        SiteSettingsTestHelper.createStorageAccessExceptions();
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startStorageAccessSettingsActivity(
                        SiteSettingsTestHelper.getStorageAccessSite());

        waitForView(withText("secondary1.com"));
        waitForView(withText("secondary3.com"));

        // Reset first permission.
        getImageViewWidget("secondary1.com").check(matches(isDisplayed())).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary1.com")));
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                });

        waitForNoView(withText("secondary1.com"));
        onView(withText("secondary3.com")).check(matches(isDisplayed()));

        // Reset second permission.
        getImageViewWidget("secondary3.com").check(matches(isDisplayed())).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            ContentSetting.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL("https://primary.com"),
                                    new GURL("https://secondary3.com")));
                });

        // Check that, because there aren't any permissions to show, the activity is closed.
        Assert.assertTrue(settingsActivity.isFinishing());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testExpectedCookieButtonsCheckedWhenFpsUiEnabled() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        waitForCookieToggleToBeBound(settingsActivity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    CookieSettingsPreference cookieToggle =
                            preferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);

                    clickButtonAndVerifyItsChecked(cookieToggle, CookieControlsMode.INCOGNITO_ONLY);
                    clickButtonAndVerifyItsChecked(
                            cookieToggle, CookieControlsMode.BLOCK_THIRD_PARTY);
                });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287320
    public void testExpectedCookieButtonsCheckedWhenFpsUiAndPrivacySandboxSettings4Enabled() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        waitForCookieToggleToBeBound(settingsActivity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    CookieSettingsPreference cookieToggle =
                            preferences.findPreference(SingleCategorySettings.COOKIE_TOGGLE);

                    clickButtonAndVerifyItsChecked(cookieToggle, CookieControlsMode.INCOGNITO_ONLY);
                    clickButtonAndVerifyItsChecked(
                            cookieToggle, CookieControlsMode.BLOCK_THIRD_PARTY);
                });

        settingsActivity.finish();
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

    @SuppressWarnings({"rawtypes", "unchecked"})
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
}
