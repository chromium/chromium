// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings.PREF_FORCE_ENABLE_ZOOM;
import static org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings.PREF_IMAGE_DESCRIPTIONS;

import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.provider.Settings;
import android.view.View;

import androidx.preference.Preference;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.accessibility.PageZoomPreference;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Tests for the Accessibility Settings menu.
 *
 * <p>TODO(crbug.com/40214849): This tests the class in //components/browser_ui, but we don't have a
 * good way of testing with native code there.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.DisableFeatures({
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS,
    ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2,
    ContentFeatureList.SMART_ZOOM
})
public class AccessibilitySettingsTest {
    private AccessibilitySettings mAccessibilitySettings;
    private PageZoomPreference mPageZoomPref;

    @Rule
    public SettingsActivityTestRule<AccessibilitySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccessibilitySettings.class);

    @Before
    public void setUp() {
        // Enable screen reader to display all settings options.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsScreenReaderEnabledForTesting(true));

        mSettingsActivityTestRule.startSettingsActivity();
        mAccessibilitySettings = mSettingsActivityTestRule.getFragment();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsScreenReaderEnabledForTesting(false));
    }

    // Generic AccessibilitySettings tests (no feature flag dependency).

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testForceEnableZoom() {
        ChromeSwitchPreference forceEnableZoomPref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(PREF_FORCE_ENABLE_ZOOM);
        Assert.assertNotNull(forceEnableZoomPref);
        Assert.assertNotNull(forceEnableZoomPref.getOnPreferenceChangeListener());

        // First scroll to the Force Enable Zoom preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.force_enable_zoom_title))));
        onView(withText(R.string.force_enable_zoom_title)).perform(click());
        Assert.assertTrue(
                "Force enable zoom option was not toggled", forceEnableZoomPref.isChecked());

        // Assert that UserPref was updated.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "Force enable zoom user pref was not updated on toggle",
                                mAccessibilitySettings.mFontSizePrefs.getForceEnableZoom()));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testCaptionPreferences() {
        Preference captionsPref =
                mAccessibilitySettings.findPreference(AccessibilitySettings.PREF_CAPTIONS);
        Assert.assertNotNull(captionsPref);
        Assert.assertNotNull(captionsPref.getOnPreferenceClickListener());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                new IntentFilter(Settings.ACTION_CAPTIONING_SETTINGS), null, false);

        // First scroll to the Captions preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.accessibility_captions_title))));
        onView(withText(R.string.accessibility_captions_title)).perform(click());
        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testImageDescriptionsPreferences_Enabled() {
        Preference imageDescriptionsPref =
                mAccessibilitySettings.findPreference(PREF_IMAGE_DESCRIPTIONS);

        Assert.assertNotNull(imageDescriptionsPref);
        Assert.assertTrue(
                "Image Descriptions option should be visible", imageDescriptionsPref.isVisible());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(new IntentFilter(Intent.ACTION_MAIN), null, true);

        // First scroll to the Image Descriptions preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(R.string.image_descriptions_settings_title))));
        onView(withText(R.string.image_descriptions_settings_title)).perform(click());

        // The activity is blocked, so just wait for the ActivityMonitor to capture an Intent.
        CriteriaHelper.pollInstrumentationThread(
                () -> monitor.getHits() >= 1, "Clicking image descriptions should open subpage");

        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    // Tests related to Page Zoom feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_decrease_zoom_button)).perform(click());
        Assert.assertTrue(startingVal > mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_decreaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setZoomValueForTesting(0);
                });
        onView(withId(R.id.page_zoom_decrease_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_increase_zoom_button)).perform(click());
        Assert.assertTrue(startingVal < mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_increaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setZoomValueForTesting(
                            PageZoomUtils.PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE);
                });
        onView(withId(R.id.page_zoom_increase_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_zoomSliderUpdatesValue() {
        getPageZoomPref();
        int startingVal = mPageZoomPref.getZoomSliderForTesting().getProgress();
        onView(withId(R.id.page_zoom_slider)).perform(ViewActions.swipeRight());
        Assert.assertNotEquals(startingVal, mPageZoomPref.getZoomSliderForTesting().getProgress());
    }

    // Tests related to Page Zoom Enhancements (fast-follow) feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS})
    public void testPageZoomPreference_savedZoomLevelsPreference_visibleWhenEnabled() {
        Preference zoomInfoPref =
                mAccessibilitySettings.findPreference(AccessibilitySettings.PREF_ZOOM_INFO);
        Assert.assertNotNull(zoomInfoPref);
        Assert.assertNotNull(zoomInfoPref.getOnPreferenceClickListener());

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(new IntentFilter(), null, false);

        // First scroll to the "Saved zoom levels" preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(R.string.zoom_info_preference_title))));
        onView(withText(R.string.zoom_info_preference_title)).perform(click());

        // The activity is blocked, so just wait for the ActivityMonitor to capture an Intent.
        CriteriaHelper.pollInstrumentationThread(
                () -> monitor.getHits() >= 1,
                "Clicking 'Saved zoom levels' should open Site Settings page.");

        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS})
    public void testPageZoomPreference_savedZoomLevelsPreference_hiddenWhenDisabled() {
        Preference zoomInfoPref =
                mAccessibilitySettings.findPreference(AccessibilitySettings.PREF_ZOOM_INFO);
        Assert.assertNotNull(zoomInfoPref);
        Assert.assertFalse(
                "Saved Zoom Levels link should not be visible when disabled",
                zoomInfoPref.isVisible());
    }

    // Tests related to Page Zoom V2 feature (OS-level adjustment experiments).

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @Features.EnableFeatures({
        ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS,
        ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2
    })
    public void testPageZoomPreference_osLevelAdjustmentPreference_visibleWhenEnabled() {
        ChromeSwitchPreference osLevelAdjustmentPref =
                (ChromeSwitchPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
        Assert.assertNotNull(osLevelAdjustmentPref);
        Assert.assertNotNull(osLevelAdjustmentPref.getOnPreferenceChangeListener());

        // Current state of the preference (on/off).
        boolean initialSettingState = osLevelAdjustmentPref.isChecked();

        // First scroll to the "Match Android font size" preference, then click.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(R.string.page_zoom_include_os_adjustment_title))));
        onView(withText(R.string.page_zoom_include_os_adjustment_title)).perform(click());

        Assert.assertTrue(
                "OS-Level adjustment setting did not change on click",
                initialSettingState != osLevelAdjustmentPref.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @DisableFeatures({ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2})
    public void testPageZoomPreference_osLevelAdjustmentPreference_hiddenWhenDisabled() {
        Preference osLevelAdjustmentPref =
                mAccessibilitySettings.findPreference(
                        AccessibilitySettings.PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
        Assert.assertNotNull(osLevelAdjustmentPref);
        Assert.assertFalse(
                "OS-Level adjustment settings should not be visible when disabled",
                osLevelAdjustmentPref.isVisible());
    }

    // Tests related to the Smart Zoom feature.

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testPageZoomPreference_smartZoom_hiddenWhenDisabled() {
        getPageZoomPref();
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_title), ViewUtils.VIEW_GONE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_summary), ViewUtils.VIEW_GONE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_current_value_text), ViewUtils.VIEW_GONE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_decrease_zoom_button), ViewUtils.VIEW_GONE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_slider), ViewUtils.VIEW_GONE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_increase_zoom_button), ViewUtils.VIEW_GONE);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_visibleWhenEnabled() {
        getPageZoomPref();
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_title), ViewUtils.VIEW_VISIBLE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_summary), ViewUtils.VIEW_VISIBLE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_current_value_text), ViewUtils.VIEW_VISIBLE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_decrease_zoom_button), ViewUtils.VIEW_VISIBLE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_slider), ViewUtils.VIEW_VISIBLE);
        ViewUtils.waitForViewCheckingState(
                withId(R.id.text_size_contrast_increase_zoom_button), ViewUtils.VIEW_VISIBLE);
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_decreaseButtonUpdatesValue() {
        getPageZoomPref();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setTextContrastValueForTesting(20);
                });
        int startingVal = mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress();
        onView(withId(R.id.text_size_contrast_decrease_zoom_button)).perform(click());
        Assert.assertTrue(
                startingVal > mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_decreaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setTextContrastValueForTesting(0);
                });
        onView(withId(R.id.text_size_contrast_decrease_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_increaseButtonUpdatesValue() {
        getPageZoomPref();

        int startingVal = mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress();
        onView(withId(R.id.text_size_contrast_increase_zoom_button)).perform(click());
        Assert.assertTrue(
                startingVal < mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress());
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_increaseButtonProperlyDisabled() {
        getPageZoomPref();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageZoomPref.setTextContrastValueForTesting(
                            PageZoomUtils.TEXT_SIZE_CONTRAST_MAX_LEVEL);
                });
        onView(withId(R.id.text_size_contrast_increase_zoom_button)).check(matches(sDisabled));
    }

    @Test
    @SmallTest
    @Feature({"Accessibility"})
    @EnableFeatures({ContentFeatureList.SMART_ZOOM})
    public void testPageZoomPreference_smartZoom_zoomSliderUpdatesValue() {
        getPageZoomPref();
        int startingVal = mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress();
        onView(withId(R.id.text_size_contrast_slider)).perform(ViewActions.swipeRight());
        Assert.assertNotEquals(
                startingVal, mPageZoomPref.getTextSizeContrastSliderForTesting().getProgress());
    }

    // Helper methods.

    private static final BaseMatcher<View> sDisabled =
            new BaseMatcher<View>() {
                @Override
                public boolean matches(Object o) {
                    return !((ChromeImageButton) o).isEnabled();
                }

                @Override
                public void describeTo(Description description) {
                    description.appendText("View was enabled, but should have been disabled.");
                }
            };

    private void getPageZoomPref() {
        mPageZoomPref =
                (PageZoomPreference)
                        mAccessibilitySettings.findPreference(
                                AccessibilitySettings.PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        Assert.assertNotNull(mPageZoomPref);
        Assert.assertTrue("Page Zoom pref should be visible.", mPageZoomPref.isVisible());
    }
}
