// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Bundle;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link ImageDescriptionsSettings} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ImageDescriptionsSettingsTest {
    // Test output error messages and widget names
    private static final String CONTENT_ERROR = "Content mismatch / error";
    private static final String VISIBILITY_ERROR = "should be visible";
    private static final String ENABLED_ERROR = "should be enabled";
    private static final String DISABLED_ERROR = "should be disabled";
    private static final String CHECKED_ERROR = "should be checked";
    private static final String UNCHECKED_ERROR = "should be unchecked";

    private static final String TOGGLE = "Image Descriptions toggle ";
    private static final String ONLY_ON_WIFI = "onlyOnWifi radio button ";
    private static final String USE_MOBILE_DATA = "useMobileData radio button ";

    @Rule
    public SettingsActivityTestRule<ImageDescriptionsSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ImageDescriptionsSettings.class);

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock private ImageDescriptionsControllerDelegate mDelegate;

    private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ImageDescriptionsController.getInstance().setDelegateForTesting(mDelegate);
    }

    private ChromeSwitchPreference mDescriptionsSwitch;
    private RadioButtonGroupAccessibilityPreference mDataPolicyPreference;

    private void launchSettings() {
        mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ImageDescriptionsSettings mImageDescriptionsSettings =
                mSettingsActivityTestRule.getFragment();

        mDescriptionsSwitch =
                mImageDescriptionsSettings.findPreference(
                        ImageDescriptionsSettings.IMAGE_DESCRIPTIONS);
        mDataPolicyPreference =
                mImageDescriptionsSettings.findPreference(
                        ImageDescriptionsSettings.IMAGE_DESCRIPTIONS_DATA_POLICY);
    }

    // Helper method for fetching PrefService

    private PrefService getPrefService() {
        mProfile = ProfileManager.getLastUsedRegularProfile();
        return UserPrefs.get(mProfile);
    }

    // Helper methods for assertions

    private void assertVisibleEnabledAndChecked(View view, String widgetName) {
        Assert.assertEquals(widgetName + VISIBILITY_ERROR, View.VISIBLE, view.getVisibility());
        Assert.assertTrue(widgetName + ENABLED_ERROR, view.isEnabled());
        Assert.assertTrue(
                widgetName + CHECKED_ERROR, ((RadioButtonWithDescription) view).isChecked());
    }

    private void assertVisibleEnabledAndUnchecked(View view, String widgetName) {
        Assert.assertEquals(widgetName + VISIBILITY_ERROR, View.VISIBLE, view.getVisibility());
        Assert.assertTrue(widgetName + ENABLED_ERROR, view.isEnabled());
        Assert.assertFalse(
                widgetName + UNCHECKED_ERROR, ((RadioButtonWithDescription) view).isChecked());
    }

    private void assertVisibleDisabledAndChecked(View view, String widgetName) {
        Assert.assertEquals(widgetName + VISIBILITY_ERROR, View.VISIBLE, view.getVisibility());
        Assert.assertFalse(widgetName + DISABLED_ERROR, view.isEnabled());
        Assert.assertTrue(
                widgetName + CHECKED_ERROR, ((RadioButtonWithDescription) view).isChecked());
    }

    private void assertVisibleDisabledAndUnchecked(View view, String widgetName) {
        Assert.assertEquals(widgetName + VISIBILITY_ERROR, View.VISIBLE, view.getVisibility());
        Assert.assertFalse(widgetName + DISABLED_ERROR, view.isEnabled());
        Assert.assertFalse(
                widgetName + UNCHECKED_ERROR, ((RadioButtonWithDescription) view).isChecked());
    }

    // Helper methods for driving UI

    private void switchToggle() {
        onView(withId(R.id.switchWidget)).perform(click());
    }

    private ViewInteraction onlyOnWifiRadioButton() {
        return onView(withId(R.id.image_descriptions_settings_only_on_wifi_radio_button));
    }

    private ViewInteraction useMobileDataRadioButton() {
        return onView(withId(R.id.image_descriptions_settings_mobile_data_radio_button));
    }

    @Test
    @SmallTest
    public void testContent() {
        launchSettings();

        Assert.assertNotNull("Image Descriptions toggle should not be null", mDescriptionsSwitch);
        Assert.assertNotNull("Data Policy radio buttons should not be null", mDataPolicyPreference);
        Assert.assertNotNull(
                "Image Descriptions toggle should have a preference change listener",
                mDescriptionsSwitch.getOnPreferenceChangeListener());
        Assert.assertNotNull(
                "Data Policy radio buttons should have a preference change listener",
                mDataPolicyPreference.getOnPreferenceChangeListener());

        Assert.assertEquals(
                CONTENT_ERROR, "Get image descriptions", mDescriptionsSwitch.getTitle());
        Assert.assertEquals(
                CONTENT_ERROR,
                "Some images are sent to Google to improve descriptions for you",
                mDescriptionsSwitch.getSummary());

        onlyOnWifiRadioButton()
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    CONTENT_ERROR,
                                    "Only on Wi-Fi",
                                    ((RadioButtonWithDescription) view).getPrimaryText());
                            Assert.assertEquals(
                                    CONTENT_ERROR,
                                    "",
                                    ((RadioButtonWithDescription) view).getDescriptionText());
                        });
        useMobileDataRadioButton()
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    CONTENT_ERROR,
                                    "Use mobile data",
                                    ((RadioButtonWithDescription) view).getPrimaryText());
                            Assert.assertEquals(
                                    CONTENT_ERROR,
                                    "Wi-Fi is used when available",
                                    ((RadioButtonWithDescription) view).getDescriptionText());
                        });
    }

    @Test
    @SmallTest
    public void testInitialState_RadioGroupDisabled() {
        // When Switch is disabled, then the radio button group should also be disabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
                    getPrefService().setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, true);
                });
        launchSettings();

        Assert.assertFalse(TOGGLE + DISABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Radio button group should be visible, disabled, and unchecked.
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleDisabledAndChecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleDisabledAndUnchecked(view, USE_MOBILE_DATA));
    }

    @Test
    @SmallTest
    public void testInitialState_RadioGroupEnabled() {
        // When Switch is enabled, then the radio button group should also be enabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, false);
                });
        launchSettings();

        Assert.assertTrue(TOGGLE + ENABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Radio button group should be visible, enabled, and unchecked.
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, USE_MOBILE_DATA));
    }

    @Test
    @SmallTest
    public void testUserTogglesSwitch_On() {
        // When we toggle switch to On, it should enable radio buttons and descriptions
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
                    getPrefService().setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, true);
                });
        launchSettings();

        Assert.assertFalse(TOGGLE + DISABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Toggle the image descriptions setting On, verify controller is updated
        switchToggle();
        verify(mDelegate, times(1)).enableImageDescriptions(mProfile);
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(true, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());

        Assert.assertTrue(TOGGLE + ENABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Radio button group should be visible, enabled, and unchecked.
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, USE_MOBILE_DATA));
    }

    @Test
    @SmallTest
    public void testUserTogglesSwitch_Off() {
        // When we toggle switch to Off, it should disable radio buttons and descriptions
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, false);
                });
        launchSettings();

        Assert.assertTrue(TOGGLE + ENABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Toggle the image descriptions setting Off, verify controller is updated
        switchToggle();
        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, times(1)).disableImageDescriptions(mProfile);
        verify(mDelegate, never()).setOnlyOnWifiRequirement(anyBoolean(), any());
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());

        Assert.assertFalse(TOGGLE + DISABLED_ERROR, mDescriptionsSwitch.isChecked());

        // Radio button group should be visible, disabled, and unchecked.
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleDisabledAndUnchecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleDisabledAndChecked(view, USE_MOBILE_DATA));
    }

    @Test
    @SmallTest
    public void testUserSelectsRadioButton_onlyOnWifi() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, false);
                });
        launchSettings();

        // Toggle should be on, radio buttons visible, enabled, and onlyOnWifi unchecked
        Assert.assertTrue(TOGGLE + ENABLED_ERROR, mDescriptionsSwitch.isChecked());
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, USE_MOBILE_DATA));

        // User selects the onlyOnWifi radio button
        onlyOnWifiRadioButton().perform(click());

        // Radio buttons should remain visible, enabled, but checked status switched
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, USE_MOBILE_DATA));

        // Verify controller was updated
        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(true, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());
    }

    @Test
    @SmallTest
    public void testUserSelectsRadioButton_useMobileData() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
                    getPrefService().setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, true);
                });
        launchSettings();

        // Toggle should be on, radio buttons visible, enabled, and useMobileData unchecked
        Assert.assertTrue(TOGGLE + ENABLED_ERROR, mDescriptionsSwitch.isChecked());
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, USE_MOBILE_DATA));

        // User selects the onlyOnWifi radio button
        useMobileDataRadioButton().perform(click());

        // Radio buttons should remain visible, enabled, but checked status switched
        onlyOnWifiRadioButton()
                .check((view, e) -> assertVisibleEnabledAndUnchecked(view, ONLY_ON_WIFI));
        useMobileDataRadioButton()
                .check((view, e) -> assertVisibleEnabledAndChecked(view, USE_MOBILE_DATA));

        // Verify controller was updated
        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(false, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());
    }
}
