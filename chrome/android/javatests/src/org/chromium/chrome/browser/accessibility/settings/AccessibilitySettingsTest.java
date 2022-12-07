// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;

import androidx.preference.Preference;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.browser_ui.accessibility.TextScalePreference;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.UiUtils;

import java.text.NumberFormat;

/**
 * Tests for the Accessibility Settings menu.
 *
 * TODO(crbug.com/1296642): This tests the class in //components/browser_ui, but we don't have a
 * good way of testing with native code there.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AccessibilitySettingsTest {
    private static final String PREF_IMAGE_DESCRIPTIONS = "image_descriptions";

    @Rule
    public SettingsActivityTestRule<AccessibilitySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccessibilitySettings.class);

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
            ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(false);
        });
    }

    /**
     * Tests setting FontScaleFactor and ForceEnableZoom in AccessibilitySettings and ensures
     * that ForceEnableZoom changes corresponding to FontScaleFactor.
     */
    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM})
    public void testAccessibilitySettings() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();

        TextScalePreference textScalePref =
                (TextScalePreference) accessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_TEXT_SCALE);
        ChromeBaseCheckBoxPreference forceEnableZoomPref =
                (ChromeBaseCheckBoxPreference) accessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_FORCE_ENABLE_ZOOM);
        NumberFormat percentFormat = NumberFormat.getPercentInstance();
        // Arbitrary value 0.4f to be larger and smaller than threshold.
        float fontSmallerThanThreshold =
                FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER - 0.4f;
        float fontBiggerThanThreshold = FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER + 0.4f;

        // Set the textScaleFactor above the threshold.
        userSetTextScale(accessibilitySettings, textScalePref, fontBiggerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since above the threshold, this will check the force enable zoom button.
        Assert.assertEquals(
                percentFormat.format(fontBiggerThanThreshold), textScalePref.getAmountForTesting());
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontBiggerThanThreshold);

        // Set the textScaleFactor below the threshold.
        userSetTextScale(accessibilitySettings, textScalePref, fontSmallerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since below the threshold and userSetForceEnableZoom is false, this will uncheck
        // the force enable zoom button.
        Assert.assertEquals(percentFormat.format(fontSmallerThanThreshold),
                textScalePref.getAmountForTesting());
        Assert.assertFalse(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(false, fontSmallerThanThreshold);

        userSetTextScale(accessibilitySettings, textScalePref, fontBiggerThanThreshold);
        // Sets onUserSetForceEnableZoom to be true.
        userSetForceEnableZoom(accessibilitySettings, forceEnableZoomPref, true);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since userSetForceEnableZoom is true, when the text scale is moved below the threshold
        // ForceEnableZoom should remain checked.
        userSetTextScale(accessibilitySettings, textScalePref, fontSmallerThanThreshold);
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontSmallerThanThreshold);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM})
    public void testChangedFontPrefSavedOnStop() {
        mSettingsActivityTestRule.startSettingsActivity();
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();

        TextScalePreference textScalePref =
                accessibilitySettings.findPreference(AccessibilitySettings.PREF_TEXT_SCALE);

        // Change text scale a couple of times.
        userSetTextScale(accessibilitySettings, textScalePref, 0.5f);
        userSetTextScale(accessibilitySettings, textScalePref, 1.75f);

        Assert.assertEquals("Histogram should not be recorded yet.", 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FontSizePrefs.FONT_SIZE_CHANGE_HISTOGRAM));

        // Simulate activity stopping.
        TestThreadUtils.runOnUiThreadBlocking(() -> accessibilitySettings.onStop());

        Assert.assertEquals("Histogram should have been recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FontSizePrefs.FONT_SIZE_CHANGE_HISTOGRAM));
        Assert.assertEquals("Histogram should have recorded final value.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        FontSizePrefs.FONT_SIZE_CHANGE_HISTOGRAM, 175));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM})
    public void testUnchangedFontPrefNotSavedOnStop() {
        mSettingsActivityTestRule.startSettingsActivity();
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();

        // Simulate activity stopping.
        TestThreadUtils.runOnUiThreadBlocking(() -> accessibilitySettings.onStop());
        Assert.assertEquals("Histogram should not have been recorded.", 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FontSizePrefs.FONT_SIZE_CHANGE_HISTOGRAM));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testCaptionPreferences() {
        mSettingsActivityTestRule.startSettingsActivity();
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();

        Preference captionsPref =
                accessibilitySettings.findPreference(AccessibilitySettings.PREF_CAPTIONS);
        Assert.assertNotNull(captionsPref);
        Assert.assertNotNull(captionsPref.getOnPreferenceClickListener());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Settings.ACTION_CAPTIONING_SETTINGS), null, false);

        // First scroll to bottom of the page, then click.
        onView(ViewMatchers.isRoot()).perform(swipeUp());
        onView(withText(org.chromium.chrome.R.string.accessibility_captions_title))
                .perform(click());
        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testImageDescriptionsPreferences_Enabled() {
        // Enable touch exploration to display settings option.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
            ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(true);
        });

        mSettingsActivityTestRule.startSettingsActivity();
        AccessibilitySettings accessibilitySettings = mSettingsActivityTestRule.getFragment();

        Preference imageDescriptionsPref =
                accessibilitySettings.findPreference(PREF_IMAGE_DESCRIPTIONS);

        Assert.assertNotNull(imageDescriptionsPref);
        Assert.assertTrue(
                "Image Descriptions option should be visible", imageDescriptionsPref.isVisible());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Intent.ACTION_MAIN), null, true);

        // First scroll to bottom of the page, then click.
        onView(ViewMatchers.isRoot()).perform(swipeUp());
        onView(withText(org.chromium.chrome.R.string.image_descriptions_settings_title))
                .perform(click());

        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertEquals(
                "Clicking image descriptions should open subpage", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    private void assertFontSizePrefs(
            final boolean expectedForceEnableZoom, final float expectedFontScale) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FontSizePrefs fontSizePrefs =
                    FontSizePrefs.getInstance(Profile.getLastUsedRegularProfile());
            Assert.assertEquals(expectedForceEnableZoom, fontSizePrefs.getForceEnableZoom());
            Assert.assertEquals(expectedFontScale, fontSizePrefs.getFontScaleFactor(), 0.001f);
        });
    }

    private static void userSetTextScale(final AccessibilitySettings accessibilitySettings,
            final TextScalePreference textScalePref, final float textScale) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> accessibilitySettings.onPreferenceChange(textScalePref, textScale));
    }

    private static void userSetForceEnableZoom(final AccessibilitySettings accessibilitySettings,
            final ChromeBaseCheckBoxPreference forceEnableZoomPref, final boolean enabled) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> accessibilitySettings.onPreferenceChange(forceEnableZoomPref, enabled));
    }
}
