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

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickImageButtonNextToText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
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
    private static final String SITE_NAME_1 = "first.com";
    private static final String SITE_NAME_2 = "second.com";

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

    @Rule
    public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

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

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private String generateSiteFromNr(int nr) {
        return "site-" + nr + ".com";
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
    @Feature({"RenderTest"})
    public void testRenderFledgePopulated() throws IOException {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();
        mRenderTestRule.render(getFledgeRootView(), "fledge_page_populated");
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
    public void testTurnFledgeOnWhenSitesListEmpty() {
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
    public void testTurnFledgeOnWhenSitesListPopulated() {
        setFledgePrefEnabled(false);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        // Check that the sites list is not displayed when Fledge is disabled.
        onView(withText(SITE_NAME_1)).check(doesNotExist());
        onView(withText(SITE_NAME_2)).check(doesNotExist());

        // Click on the toggle.
        onView(getFledgeToggleMatcher()).perform(click());

        // Check that the all sites pref is displayed
        onViewWaiting(withText(R.string.settings_fledge_page_see_all_sites_label))
                .check(matches(isDisplayed()));

        // Check that the sites list is displayed when Fledge is enabled.
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));
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

    @Test
    @SmallTest
    public void testPopulateSitesList() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMaxDisplayedSites() {
        setFledgePrefEnabled(true);
        for (int i = 0; i < FledgeFragmentV4.MAX_DISPLAYED_SITES + 1; i++) {
            mFakePrivacySandboxBridge.setFledgeJoiningAllowed(generateSiteFromNr(i), true);
        }
        startFledgeSettings();

        // Scroll to pref below last displayed site.
        scrollToSetting(withText(R.string.settings_fledge_page_see_all_sites_label));

        // Verify that only MAX_DISPLAY_SITES are shown.
        onView(withText(generateSiteFromNr(FledgeFragmentV4.MAX_DISPLAYED_SITES - 1)))
                .check(matches(isDisplayed()));
        onView(withText(generateSiteFromNr(FledgeFragmentV4.MAX_DISPLAYED_SITES)))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testRemoveSitesFromList() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        // Remove the first site from the list and check that it is blocked.
        clickImageButtonNextToText(SITE_NAME_1);
        onView(withText(SITE_NAME_1)).check(doesNotExist());
        assertThat(PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                hasItem(SITE_NAME_1));

        // Remove the second site from the list and check that it is blocked.
        clickImageButtonNextToText(SITE_NAME_2);
        onView(withText(SITE_NAME_2)).check(doesNotExist());
        assertThat(PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay(),
                hasItem(SITE_NAME_2));

        // Check that the empty state UI is displayed when the sites list is empty.
        onView(withText(R.string.settings_fledge_page_current_sites_description_empty))
                .check(matches(isDisplayed()));
    }
    // TODO(http://b/261823248): Add Managed state tests when the Privacy Sandbox policy is
    // implemented.
}
