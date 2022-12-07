// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.annotation.StringRes;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/**
 * Tests {@link AdMeasurementFragmentV4}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class AdMeasurementFragmentV4Test {
    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public SettingsActivityTestRule<AdMeasurementFragmentV4> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AdMeasurementFragmentV4.class);

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
        });
    }

    private void startAdMeasuremenSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.settings_ad_measurement_page_title));
    }

    private Matcher<View> getAdMeasurementToggleMatcher() {
        return allOf(withId(R.id.switchWidget),
                withParent(withParent(hasDescendant(
                        withText(R.string.settings_ad_measurement_page_toggle_label)))));
    }

    private View getRootView(@StringRes int text) {
        View[] view = {null};
        onView(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    private void setAdMeasurementPrefEnabled(boolean isEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AdMeasurementFragmentV4.setAdMeasurementPrefEnabled(isEnabled));
    }

    private boolean isAdMeasurementPrefEnabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> AdMeasurementFragmentV4.isAdMeasurementPrefEnabled());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderAdMeasurement() throws IOException {
        setAdMeasurementPrefEnabled(true);
        startAdMeasuremenSettings();
        mRenderTestRule.render(getRootView(R.string.settings_ad_measurement_page_title),
                "ad_measurement_page_toggle_on");
    }

    @Test
    @SmallTest
    public void testToggleUncheckedWhenAdMeasurementOff() {
        setAdMeasurementPrefEnabled(false);
        startAdMeasuremenSettings();
        onView(getAdMeasurementToggleMatcher()).check(matches(not(isChecked())));
    }

    @Test
    @SmallTest
    public void testToggleCheckedWhenAdMeasurementOn() {
        setAdMeasurementPrefEnabled(true);
        startAdMeasuremenSettings();
        onView(getAdMeasurementToggleMatcher()).check(matches(isChecked()));
    }

    @Test
    @SmallTest
    public void testTurnAdMeasurementOn() {
        setAdMeasurementPrefEnabled(false);
        startAdMeasuremenSettings();
        onView(getAdMeasurementToggleMatcher()).perform(click());

        assertTrue(isAdMeasurementPrefEnabled());
    }

    @Test
    @SmallTest
    public void testTurnAdMeasurementOff() {
        setAdMeasurementPrefEnabled(true);
        startAdMeasuremenSettings();
        onView(getAdMeasurementToggleMatcher()).perform(click());

        assertFalse(isAdMeasurementPrefEnabled());
    }
    // TODO(http://b/261439615): Add Managed state tests when the Privacy Sandbox policy is
    // implemented.
}
