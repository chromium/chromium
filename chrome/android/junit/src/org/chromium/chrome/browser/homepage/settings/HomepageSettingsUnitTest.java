// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.view.View;
import android.widget.TextView;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.core.app.ActivityScenario;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomeButtonStatus;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Test for {@link HomepageSettings} to check the UI components and the interactions. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class})
@Features.EnableFeatures({
    ChromeFeatureList.SHOW_HOME_BUTTON_POLICY_ANDROID,
    ChromeFeatureList.HOMEPAGE_IS_NEW_TAB_PAGE_POLICY_ANDROID
})
public class HomepageSettingsUnitTest {
    private static final String ASSERT_MESSAGE_SWITCH_ENABLE = "Switch should be enabled.";
    private static final String ASSERT_MESSAGE_SWITCH_DISABLE = "Switch should be disabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_ENABLED =
            "RadioButton should be enabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_DISABLED =
            "RadioButton should be disabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_GONE =
            "RadioButton should be View.GONE.";
    private static final String ASSERT_MESSAGE_TITLE_ENABLED =
            "Title for RadioButtonGroup should be enabled.";
    private static final String ASSERT_MESSAGE_TITLE_DISABLED =
            "Title for RadioButtonGroup should be disabled.";

    private static final String ASSERT_MESSAGE_SWITCH_CHECK =
            "Switch preference state is not consistent with test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK =
            "NTP Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK =
            "Customized Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_EDIT_TEXT =
            "EditText does not contains the expected homepage in test settings.";
    private static final String ASSERT_HOMEPAGE_MANAGER_SETTINGS =
            "HomepageManager#getHomepageGurl is different than test homepage settings.";

    private static final String ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH =
            "HomepageLocationType is different than test settings.";
    private static final String ASSERT_HOME_SWITCH_STATUS_MISMATCH =
            "HomeButtonStatus is different than test settings.";

    private static final String TEST_URL_FOO = JUnitTestGURLs.URL_1.getSpec();
    private static final String TEST_URL_BAR = JUnitTestGURLs.URL_2.getSpec();
    private static final String CHROME_NTP = JUnitTestGURLs.NTP_URL.getSpec();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public HomepagePolicyManager mMockHomepagePolicyManager;
    @Mock public PartnerBrowserCustomizations mMockPartnerBrowserCustomizations;
    @Mock public Profile mProfile;

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;

    private UserActionTester mActionTester;

    private ChromeSwitchPreference mSwitch;
    private RadioButtonGroupHomepagePreference mRadioGroupPreference;

    private TextView mTitleTextView;
    private RadioButtonWithDescription mChromeNtpRadioButton;
    private RadioButtonWithEditText mCustomUriRadioButton;

    @Before
    public void setUp() {
        HomepagePolicyManager.setInstanceForTests(mMockHomepagePolicyManager);
        PartnerBrowserCustomizations.setInstanceForTesting(mMockPartnerBrowserCustomizations);
        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(
                activity -> {
                    mActivity = activity;
                    // Needed for HomepageSettings to inflate correctly.
                    mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
                });
        mActionTester = new UserActionTester();
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        HomepagePolicyManager.setPrefServiceForTesting(Mockito.mock(PrefService.class));
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        mActionTester.tearDown();
    }

    private void launchHomepageSettings() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        HomepageSettings settings =
                (HomepageSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        HomepageSettings.class.getClassLoader(),
                                        HomepageSettings.class.getName());
        settings.setProfile(mProfile);
        fragmentManager.beginTransaction().replace(android.R.id.content, settings).commit();

        mActivityScenario.moveToState(State.STARTED);
        mSwitch = settings.getHomepageSwitchForTesting();
        mRadioGroupPreference = settings.getHomepageRadioGroupForTesting();

        Assert.assertTrue(
                "RadioGroupPreference should be visible when Homepage Conversion is enabled.",
                mRadioGroupPreference.isVisible());
        assertThat(
                "Title text view is null.",
                mRadioGroupPreference.getTitleTextView(),
                Matchers.notNullValue());
        assertThat(
                "Chrome NTP radio button is null.",
                mRadioGroupPreference.getChromeNtpRadioButton(),
                Matchers.notNullValue());
        assertThat(
                "Custom URI radio button is null.",
                mRadioGroupPreference.getCustomUriRadioButton(),
                Matchers.notNullValue());

        mTitleTextView = mRadioGroupPreference.getTitleTextView();
        mChromeNtpRadioButton = mRadioGroupPreference.getChromeNtpRadioButton();
        mCustomUriRadioButton = mRadioGroupPreference.getCustomUriRadioButton();
    }

    private void finishSettingsActivity() {
        mActivityScenario.moveToState(State.DESTROYED);
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_ChromeNtp() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.useChromeNtpForTest();

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_ChromeNtp_WithPartner() {
        setPartnerHomepage(TEST_URL_FOO);
        mHomepageTestRule.useChromeNtpForTest();

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_Customized() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_Policies_Customized() {
        setHomepageLocationPolicy(new GURL(TEST_URL_BAR));

        launchHomepageSettings();

        // When policy enabled, all components should be disabled.
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Additional verification - text message should be displayed, NTP button should be hidden.
        Assert.assertEquals(
                "NTP Button should not be visible.",
                View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals(
                "Customized Button should be visible.",
                View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_Policies_NTP() {
        setHomepageLocationPolicy(new GURL(CHROME_NTP));

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());

        // Additional verification - customized radio button should be disabled.
        Assert.assertEquals(
                "NTP Button should be visible.",
                View.VISIBLE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals(
                "Customized Button should not be visible.",
                View.GONE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testShowHomeButton_Policy_On() {
        setShowHomeButtonPolicy(true);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());

        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testShowHomeButton_Policy_Off() {
        setShowHomeButtonPolicy(false);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());

        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_OFF,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testShowHomeButton_Recommended_Following() {
        // Mock that the policy is recommended and user's setting matches the recommendation.
        setShowHomeButtonRecommendation(true);
        // Pre-set user's preference to be ON, following the recommendation.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);

        launchHomepageSettings();
        // Switch should be enabled and checked.
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
    }

    @Test
    @Feature({"Homepage"})
    public void testShowHomeButton_Recommended_NotFollowing() {
        // Mock that the policy is recommended and user's setting does not match the
        // recommendation.
        setShowHomeButtonRecommendation(false);
        // Pre-set user's preference to be OFF, not following the recommendation to be ON.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);

        launchHomepageSettings();
        // Switch should be enabled and unchecked.
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        // Toggling the switch should work as normal
        mSwitch.performClick();
        Assert.assertTrue(mSwitch.isChecked());
    }

    @Test
    @Feature({"Homepage"})
    public void testHomepageIsNtp_Policy_On() {
        setHomepageIsNtpPolicy(true);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testHomepageIsNtp_Policy_On_Customized() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        setHomepageIsNtpPolicy(true);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testHomepageIsNtp_Policy_Off() {
        setHomepageIsNtpPolicy(false);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mChromeNtpRadioButton.getVisibility());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testHomepageIsNtp_Policy_Off_Customized() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        setHomepageIsNtpPolicy(false);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mChromeNtpRadioButton.getVisibility());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_ShowHomeButtonOFF_HomepageLocationON() {
        setShowHomeButtonPolicy(false);
        setHomepageLocationPolicy(new GURL(TEST_URL_BAR));

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());

        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Additional verification - text message should be displayed, NTP button should be hidden.
        Assert.assertEquals(
                "NTP Button should not be visible.",
                View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals(
                "Customized Button should be visible.",
                View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_OFF,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_ShowHomeButtonON_HomepageLocationON() {
        setShowHomeButtonPolicy(true);
        setHomepageLocationPolicy(new GURL(TEST_URL_BAR));

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());

        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Additional verification - text message should be displayed, NTP button should be hidden.
        Assert.assertEquals(
                "NTP Button should not be visible.",
                View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals(
                "Customized Button should be visible.",
                View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_ShowHomeButtonOFF_HomepageIsNtpOFF() {
        setShowHomeButtonPolicy(false);
        setHomepageIsNtpPolicy(false);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mChromeNtpRadioButton.getVisibility());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_OFF,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_ShowHomeButtonON_HomepageIsNtpOFF() {
        setShowHomeButtonPolicy(true);
        setHomepageIsNtpPolicy(false);

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mChromeNtpRadioButton.getVisibility());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_HomepageIsNtpOFF_HomepageLocationON() {
        setHomepageIsNtpPolicy(false);
        setHomepageLocationPolicy(new GURL(TEST_URL_BAR));

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mChromeNtpRadioButton.getVisibility());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertFalse(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testPolicies_HomepageIsNtpON_HomepageLocationON() {
        setHomepageIsNtpPolicy(true);
        setHomepageLocationPolicy(new GURL(TEST_URL_BAR));

        launchHomepageSettings();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_RADIO_BUTTON_GONE, View.GONE, mCustomUriRadioButton.getVisibility());

        Assert.assertEquals(
                "Disclaimer should be gone.",
                View.GONE,
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text)
                        .getVisibility());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageManager.getHomepageCharacterizationHelper().isNtp());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.POLICY_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_DefaultToPartner() {
        setPartnerHomepage(TEST_URL_FOO);
        mHomepageTestRule.useDefaultHomepageForTest();

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.PARTNER_PROVIDED_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_DefaultToNtp() {
        mHomepageTestRule.useDefaultHomepageForTest();

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.DEFAULT_NTP,
                HomepageManager.getInstance().getHomepageLocationType());

        // When no default homepage provided, the string should just be empty.
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    @Test
    @Feature({"Homepage"})
    public void testStartUp_HomepageDisabled() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.disableHomepageForTest();

        launchHomepageSettings();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertTrue(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ false).isEmpty());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_OFF,
                HomepageManager.getInstance().getHomeButtonStatus());
    }

    /** Test toggle switch to enable/disable homepage. */
    @Test
    @Feature({"Homepage"})
    public void testToggleSwitch() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        mHomepageTestRule.useChromeNtpForTest();

        launchHomepageSettings();

        HomepageManager homepageManager = HomepageManager.getInstance();
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled.", homepageManager.isHomepageEnabled());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());

        // Check the widget status
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Click the switch
        mSwitch.performClick();
        Assert.assertFalse(
                "After toggle the switch, " + ASSERT_MESSAGE_TITLE_DISABLED,
                mTitleTextView.isEnabled());
        Assert.assertFalse(
                "After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(
                "After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(
                "Homepage should be disabled after toggle switch.",
                homepageManager.isHomepageEnabled());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_OFF,
                HomepageManager.getInstance().getHomeButtonStatus());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        mSwitch.performClick();
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled again.", homepageManager.isHomepageEnabled());
        Assert.assertEquals(
                ASSERT_HOME_SWITCH_STATUS_MISMATCH,
                HomeButtonStatus.USER_ON,
                HomepageManager.getInstance().getHomeButtonStatus());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Histogram for location change should not change when toggling switch preference.
        assertUserActionRecorded(false);
    }

    /** Test checking different radio button to change the homepage. */
    @Test
    @Feature({"Homepage"})
    public void testCheckRadioButtons() throws Exception {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        launchHomepageSettings();

        HomepageManager homepageManager = HomepageManager.getInstance();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                TEST_URL_FOO,
                homepageManager.getHomepageGurl(/* isIncognito= */ false).getSpec());
        assertUserActionRecorded(false);

        // Check radio button to select NTP as homepage.
        checkRadioButtonAndWait(mChromeNtpRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        assertUserActionRecorded(true);

        // Check back to customized radio button
        checkRadioButtonAndWait(mCustomUriRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // End the activity. The homepage should be the customized url.
        finishSettingsActivity();
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                TEST_URL_FOO,
                homepageManager.getHomepageGurl(/* isIncognito= */ false).getSpec());
        assertUserActionRecorded(true);
    }

    @Test
    @Feature({"Homepage"})
    public void testUI_HomepageSelection_Recommended_Following() {
        setHomepageSelectionRecommendation(true);
        launchHomepageSettings();

        // The switch and radio buttons should be enabled to allow user to override.
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());

        TextView disclaimer =
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text);
        Assert.assertEquals(
                "Disclaimer should be visible.", View.VISIBLE, disclaimer.getVisibility());
        Assert.assertEquals(
                "Disclaimer should show recommendation text.",
                mActivity.getString(R.string.recommended_by_your_organization),
                disclaimer.getText());
    }

    @Test
    @Feature({"Homepage"})
    public void testUI_HomepageSelection_Recommended_NotFollowing() {
        setHomepageSelectionRecommendation(false);
        launchHomepageSettings();

        TextView disclaimer =
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text);
        Assert.assertEquals("Disclaimer should be gone.", View.GONE, disclaimer.getVisibility());
    }

    @Test
    @Feature({"Homepage"})
    public void testUI_HomepageSelection_Unmanaged() {
        Mockito.doReturn(false)
                .when(mMockHomepagePolicyManager)
                .isHomepageSelectionPolicyRecommended();
        launchHomepageSettings();

        TextView disclaimer =
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text);
        Assert.assertEquals("Disclaimer should be gone.", View.GONE, disclaimer.getVisibility());
    }

    /**
     * Tests that the homepage preference is saved immediately when the user changes the radio
     * button selection, without waiting for the fragment's onStop() lifecycle event.
     */
    @Test
    @Feature({"Homepage"})
    public void testHomepageSavedImmediatelyOnChange() throws Exception {
        // Arrange: Start with a custom homepage.
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        launchHomepageSettings();

        // Act: Click the NTP radio button.
        checkRadioButtonAndWait(mChromeNtpRadioButton);
        ShadowLooper.idleMainLooper(); // Ensure any pending tasks are run.

        // Assert: The HomepageManager should reflect the change to NTP immediately,
        // even though finishSettingsActivity() has not been called.
        Assert.assertTrue(
                "HomepageManager should be updated to NTP immediately after the radio button"
                        + " is clicked.",
                UrlUtilities.isNtpUrl(
                        HomepageManager.getInstance().getHomepageGurl(/* isIncognito= */ false)));
    }

    /**
     * Tests that the "Recommended" disclaimer does not appear for the radio button group when a
     * higher-priority "Managed" policy is active.
     */
    @Test
    @Feature({"Homepage"})
    public void testManagedPolicySuppressesRecommendationUI() {
        // Arrange: Set both a "Managed" policy and a "Recommended" policy.
        // The managed policy should take precedence.
        setHomepageIsNtpPolicy(true); // This makes the homepage managed.
        setHomepageSelectionRecommendation(true); // A recommendation also exists.

        launchHomepageSettings();

        // Assert: The managed policy for the switch should be active.
        Assert.assertFalse("The main switch should be disabled by policy.", mSwitch.isEnabled());

        // Assert: The "Recommended" disclaimer for the radio buttons should NOT be visible,
        // because the managed policy overrides it.
        TextView disclaimer =
                mRadioGroupPreference
                        .getViewForTesting()
                        .findViewById(R.id.managed_disclaimer_text);
        Assert.assertEquals(
                "Disclaimer should be GONE when a managed policy overrides a recommendation.",
                View.GONE,
                disclaimer.getVisibility());
    }

    /** Test if changing uris in EditText will change homepage accordingly. */
    @Test
    @Feature({"Homepage"})
    public void testChangeCustomized() throws Exception {
        mHomepageTestRule.useChromeNtpForTest();
        launchHomepageSettings();

        HomepageManager homepageManager = HomepageManager.getInstance();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertTrue(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                UrlUtilities.isNtpUrl(homepageManager.getHomepageGurl(/* isIncognito= */ false)));
        assertUserActionRecorded(false);

        // Update the text box. To do this, request focus for customized radio button so that the
        // checked option will be changed.
        mCustomUriRadioButton.getEditTextForTests().requestFocus();
        mCustomUriRadioButton.setPrimaryText(TEST_URL_FOO);
        Assert.assertTrue(
                "EditText never got the focus.",
                mCustomUriRadioButton.getEditTextForTests().isFocused());

        // Radio Button should switched to customized homepage.
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());

        // Update the text box and exit the activity, homepage should change accordingly.
        mCustomUriRadioButton.setPrimaryText(TEST_URL_BAR);
        finishSettingsActivity();

        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                TEST_URL_BAR,
                homepageManager.getHomepageGurl(/* isIncognito= */ false).getSpec());
        assertUserActionRecorded(true);
    }

    private void checkRadioButtonAndWait(RadioButtonWithDescription radioButton) {
        TouchCommon.singleClickView(radioButton, 5, 5);
        ShadowLooper.idleMainLooper();
        Assert.assertTrue("RadioButton is not checked.", radioButton.isChecked());
    }

    private void setPartnerHomepage(String partnerHomepage) {
        Mockito.doReturn(true)
                .when(mMockPartnerBrowserCustomizations)
                .isHomepageProviderAvailableAndEnabled();
        Mockito.doReturn(new GURL(partnerHomepage))
                .when(mMockPartnerBrowserCustomizations)
                .getHomePageUrl();
    }

    private void setHomepageLocationPolicy(GURL homepagePolicy) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isHomepageLocationPolicyManaged();
        Mockito.doReturn(homepagePolicy)
                .when(mMockHomepagePolicyManager)
                .getHomepageLocationPolicyUrl();
    }

    private void setShowHomeButtonPolicy(Boolean val) {
        Mockito.doReturn(val != null)
                .when(mMockHomepagePolicyManager)
                .isShowHomeButtonPolicyManaged();
        Mockito.doReturn(Boolean.TRUE.equals(val))
                .when(mMockHomepagePolicyManager)
                .getShowHomeButtonPolicyValue();
    }

    private void setShowHomeButtonRecommendation(boolean isFollowing) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isShowHomeButtonPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageButtonPolicyRecommendation();
    }

    private void setHomepageIsNtpPolicy(Boolean val) {
        Mockito.doReturn(val != null)
                .when(mMockHomepagePolicyManager)
                .isHomepageIsNtpPolicyManaged();
        Mockito.doReturn(Boolean.TRUE.equals(val))
                .when(mMockHomepagePolicyManager)
                .getHomepageIsNtpPolicyValue();
    }

    private void assertUserActionRecorded(boolean recorded) {
        Assert.assertEquals(
                "User action <Settings.Homepage.LocationChanged_V2> record differently.",
                recorded,
                mActionTester.getActions().contains("Settings.Homepage.LocationChanged_V2"));
    }

    private void setHomepageSelectionRecommendation(boolean isFollowing) {
        Mockito.doReturn(true)
                .when(mMockHomepagePolicyManager)
                .isHomepageSelectionPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageSelectionPolicyRecommendation();
    }
}
