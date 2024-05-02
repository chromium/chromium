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
import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Test for {@link HomepageSettings} to check the UI components and the interactions. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class})
public class HomepageSettingsUnitTest {
    private static final String ASSERT_MESSAGE_SWITCH_ENABLE = "Switch should be enabled.";
    private static final String ASSERT_MESSAGE_SWITCH_DISABLE = "Switch should be disabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_ENABLED =
            "RadioButton should be enabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_DISABLED =
            "RadioButton should be disabled.";
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

    private static final String TEST_URL_FOO = JUnitTestGURLs.URL_1.getSpec();
    private static final String TEST_URL_BAR = JUnitTestGURLs.URL_2.getSpec();
    private static final String CHROME_NTP = JUnitTestGURLs.NTP_URL.getSpec();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public HomepagePolicyManager mMockHomepagePolicyManger;
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
        HomepagePolicyManager.setInstanceForTests(mMockHomepagePolicyManger);
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
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        mActionTester.tearDown();
    }

    private void launchHomepageSettings() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        HomepageSettings fragment =
                (HomepageSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        HomepageSettings.class.getClassLoader(),
                                        HomepageSettings.class.getName());
        fragment.setProfile(mProfile);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();

        mActivityScenario.moveToState(State.STARTED);
        mSwitch =
                (ChromeSwitchPreference)
                        fragment.findPreference(HomepageSettings.PREF_HOMEPAGE_SWITCH);
        mRadioGroupPreference =
                (RadioButtonGroupHomepagePreference)
                        fragment.findPreference(HomepageSettings.PREF_HOMEPAGE_RADIO_GROUP);

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
    @SmallTest
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
    }

    @Test
    @SmallTest
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
    }

    @Test
    @SmallTest
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
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_Policies_Customized() {
        setHomepagePolicy(new GURL(TEST_URL_BAR));

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
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_Policies_NTP() {
        setHomepagePolicy(new GURL(CHROME_NTP));

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
                ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
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
    }

    @Test
    @SmallTest
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
    }

    @Test
    @SmallTest
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
                HomepageManager.getInstance().getHomepageGurl().isEmpty());
    }

    /** Test toggle switch to enable/disable homepage. */
    @Test
    @SmallTest
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
    @SmallTest
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
                homepageManager.getHomepageGurl().getSpec());
        assertUserActionRecorded(false);

        // Check radio button to select NTP as homepage. Homepage is not changed yet at this time.
        checkRadioButtonAndWait(mChromeNtpRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        assertUserActionRecorded(false);

        // Check back to customized radio button
        checkRadioButtonAndWait(mCustomUriRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT,
                TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // End the activity. The homepage should be the customized url, and the location counter
        // should stay at 0 as nothing is changed.
        finishSettingsActivity();
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                TEST_URL_FOO,
                homepageManager.getHomepageGurl().getSpec());
        assertUserActionRecorded(false);
    }

    /** Test if changing uris in EditText will change homepage accordingly. */
    @Test
    @SmallTest
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
                UrlUtilities.isNtpUrl(homepageManager.getHomepageGurl()));
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
                homepageManager.getHomepageGurl().getSpec());
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

    private void setHomepagePolicy(GURL homepagePolicy) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManger).isHomepageLocationPolicyEnabled();
        Mockito.doReturn(homepagePolicy).when(mMockHomepagePolicyManger).getHomepagePreference();
    }

    private void assertUserActionRecorded(boolean recorded) {
        Assert.assertEquals(
                "User action <Settings.Homepage.LocationChanged_V2> record differently.",
                recorded,
                mActionTester.getActions().contains("Settings.Homepage.LocationChanged_V2"));
    }
}
