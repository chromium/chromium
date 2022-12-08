// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

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
 * Tests {@link FledgeFragmentV4}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
public final class FledgeFragmentV4Test {
    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public SettingsActivityTestRule<FledgeFragmentV4> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(FledgeFragmentV4.class);

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
        });
    }

    private void startFledgeSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(withText(R.string.settings_fledge_page_title));
    }

    private Matcher<View> getFledgeToggleMatcher() {
        return allOf(withId(R.id.switchWidget),
                withParent(withParent(
                        hasDescendant(withText(R.string.settings_fledge_page_toggle_label)))));
    }

    private View getFledgeRootView() {
        return getRootViewSanitized(R.string.settings_fledge_page_title);
    }

    private void setFledgePrefEnabled(boolean isEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> FledgeFragmentV4.setFledgePrefEnabled(isEnabled));
    }

    private boolean isFledgePrefEnabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> FledgeFragmentV4.isFledgePrefEnabled());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderFledgeOff() throws IOException {
        setFledgePrefEnabled(false);
        startFledgeSettings();
        mRenderTestRule.render(getFledgeRootView(), "fledge_page_off");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderFledgeEmpty() throws IOException {
        setFledgePrefEnabled(true);
        startFledgeSettings();
        mRenderTestRule.render(getFledgeRootView(), "fledge_page_empty");
    }

    @Test
    @SmallTest
    public void testToggleUncheckedWhenFledgeOff() {
        setFledgePrefEnabled(false);
        startFledgeSettings();
        onView(getFledgeToggleMatcher()).check(matches(not(isChecked())));
    }

    @Test
    @SmallTest
    public void testToggleCheckedWhenFledgeOn() {
        setFledgePrefEnabled(true);
        startFledgeSettings();
        onView(getFledgeToggleMatcher()).check(matches(isChecked()));
    }

    @Test
    @SmallTest
    public void testTurnFledgeOn() {
        setFledgePrefEnabled(false);
        startFledgeSettings();
        onView(getFledgeToggleMatcher()).perform(click());

        assertTrue(isFledgePrefEnabled());
        onViewWaiting(withText(R.string.settings_fledge_page_current_sites_description_empty))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_fledge_page_current_sites_description_disabled))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testTurnFledgeOff() {
        setFledgePrefEnabled(true);
        startFledgeSettings();
        onView(getFledgeToggleMatcher()).perform(click());

        assertFalse(isFledgePrefEnabled());
        onViewWaiting(withText(R.string.settings_fledge_page_current_sites_description_disabled))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_fledge_page_current_sites_description_empty))
                .check(doesNotExist());
    }
    // TODO(http://b/261823248): Add Managed state tests when the Privacy Sandbox policy it
    // implemented.
}
