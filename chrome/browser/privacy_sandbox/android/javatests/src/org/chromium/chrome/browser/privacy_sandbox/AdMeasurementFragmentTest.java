// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.hasItems;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Tests {@link AdMeasurementFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public final class AdMeasurementFragmentTest {
    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_PRIVACY_SANDBOX)
                    .build();

    @Rule
    public SettingsActivityTestRule<AdMeasurementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AdMeasurementFragment.class);

    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
                });

        mUserActionTester.tearDown();
    }

    private void startAdMeasuremenSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(
                allOf(
                        withText(R.string.settings_ad_measurement_page_title),
                        withParent(withId(R.id.action_bar))));
    }

    private Matcher<View> getAdMeasurementToggleMatcher() {
        return allOf(
                withId(R.id.switchWidget),
                withParent(
                        withParent(
                                hasDescendant(
                                        withText(
                                                R.string
                                                        .settings_ad_measurement_page_toggle_label)))));
    }

    private void setAdMeasurementPrefEnabled(boolean isEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AdMeasurementFragment.setAdMeasurementPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile(), isEnabled));
    }

    private boolean isAdMeasurementPrefEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AdMeasurementFragment.isAdMeasurementPrefEnabled(
                                ProfileManager.getLastUsedRegularProfile()));
    }

    private void scrollToSetting(Matcher<View> matcher) {
        ViewUtils.onViewWaiting(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    @Test
    @SmallTest
    public void adMeasurementDisclaimerMetrics() throws IOException {
        setAdMeasurementPrefEnabled(true);
        startAdMeasuremenSettings();
        String disclaimerText =
                mSettingsActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.settings_ad_measurement_page_disclaimer_clank);
        String matcherText = disclaimerText.replaceAll("<link>|</link>", "");
        scrollToSetting(withText(matcherText));
        onView(withText(matcherText)).check(matches(isDisplayed()));
        onView(withText(matcherText)).perform(clickOnClickableSpan(0));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.AdMeasurement.PrivacyPolicyLinkClicked"));
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
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.AdMeasurement.Enabled"));
    }

    @Test
    @SmallTest
    public void testTurnAdMeasurementOff() {
        setAdMeasurementPrefEnabled(true);
        startAdMeasuremenSettings();
        onView(getAdMeasurementToggleMatcher()).perform(click());

        assertFalse(isAdMeasurementPrefEnabled());
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.AdMeasurement.Disabled"));
    }

    @Test
    @SmallTest
    @Policies.Add({
        @Policies.Item(key = "PrivacySandboxAdMeasurementEnabled", string = "false"),
        @Policies.Item(key = "PrivacySandboxPromptEnabled", string = "false")
    })
    public void testAdMeasurementManaged() {
        startAdMeasuremenSettings();

        // Check default state and try to press the toggle.
        assertFalse(isAdMeasurementPrefEnabled());
        onView(getAdMeasurementToggleMatcher()).check(matches(not(isChecked())));
        onView(getAdMeasurementToggleMatcher()).perform(click());

        // Check that the state of the pref and the toggle did not change.
        assertFalse(isAdMeasurementPrefEnabled());
        onView(getAdMeasurementToggleMatcher()).check(matches(not(isChecked())));
    }
}
