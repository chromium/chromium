// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/** Tests family link controls are reflected in UI */
@DoNotBatch(
        reason = "Activity must be destroyed between tests to ensure the child account is removed.")
@RunWith(ChromeJUnit4ClassRunner.class)
public class FamilyLinkControlsTest {

    public final SigninTestRule mSigninTestRule = new SigninTestRule();
    private CoreAccountInfo mAccountInfo;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Rule public JniMocker mocker = new JniMocker();
    @Mock public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Initialize the browser.
        SiteSettingsTestUtils.startSiteSettingsMenu("").finish();

        ThreadUtils.runOnUiThreadBlocking(
                () -> SigninCheckerProvider.get(ProfileManager.getLastUsedRegularProfile()));
        mAccountInfo = mSigninTestRule.addChildTestAccountThenWaitForSignin();

        // Wait for SigninChecker to be initialized
        CriteriaHelper.pollUiThread(
                () ->
                        IdentityServicesProvider.get()
                                .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                .getIdentityManager()
                                .hasPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @SmallTest
    public void testDeletingOnDeviceDataBlockedForSupervisedUsers() {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.SITE_DATA);
        PreferenceFragmentCompat preferenceFragment =
                (PreferenceFragmentCompat) settingsActivity.getMainFragment();
        PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
        ChromeSwitchPreference binary_toggle = preferenceScreen.findPreference("binary_toggle");

        // When deleting cookies are blocked through Family Link, the toggle will be checked and
        // disabled
        Assert.assertTrue(binary_toggle.isChecked());
        Assert.assertFalse(binary_toggle.isEnabled());
        onView(
                        allOf(
                                withId(android.R.id.summary),
                                hasSibling(
                                        allOf(
                                                withText(
                                                        org.chromium.chrome.test.R.string
                                                                .site_data_page_title),
                                                withId(android.R.id.title)))))
                .check(
                        matches(
                                withText(
                                        containsString(
                                                settingsActivity.getString(
                                                        org.chromium.chrome.test.R.string
                                                                .managed_by_your_parent)))));
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    public void testDeletingOnDeviceDataAllowedForSupervisedUsers() throws InterruptedException {
        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        when(mWebsitePreferenceBridgeJniMock.isContentSettingManagedByCustodian(
                        any(), eq(ContentSettingsType.COOKIES)))
                .thenReturn(false);
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        any(), eq(ContentSettingsType.COOKIES)))
                .thenReturn(false);

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.SITE_DATA);
        PreferenceFragmentCompat preferenceFragment =
                (PreferenceFragmentCompat) settingsActivity.getMainFragment();
        PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
        ChromeSwitchPreference binary_toggle = preferenceScreen.findPreference("binary_toggle");

        // When deleting cookies are not blocked through Family Link the toggle will be enabled
        Assert.assertTrue(binary_toggle.isEnabled());
        settingsActivity.finish();
    }
}
