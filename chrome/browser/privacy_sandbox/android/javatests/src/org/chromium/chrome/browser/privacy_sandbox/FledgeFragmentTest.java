// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasItems;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.clickImageButtonNextToText;
import static org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxTestUtils.getRootViewSanitized;
import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
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

/** Tests {@link FledgeFragment} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class FledgeFragmentTest {
    private static final String SITE_NAME_1 = "first.com";
    private static final String SITE_NAME_2 = "second.com";

    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .setRevision(1)
                    .build();

    @Rule
    public SettingsActivityTestRule<FledgeFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(FledgeFragment.class);

    @Rule public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);

        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
                });

        mUserActionTester.tearDown();
    }

    private void startFledgeSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        ViewUtils.onViewWaiting(
                allOf(
                        withText(R.string.settings_fledge_page_title),
                        withParent(withId(R.id.action_bar))));
    }

    private Matcher<View> getFledgeToggleMatcher() {
        return allOf(
                withId(R.id.switchWidget),
                withParent(
                        withParent(
                                hasDescendant(
                                        withText(R.string.settings_fledge_page_toggle_label)))));
    }

    private View getFledgeRootView() {
        return getRootViewSanitized(R.string.settings_fledge_page_toggle_sub_label);
    }

    private View getAllSitesPageRootView() {
        return getRootViewSanitized(R.string.settings_fledge_all_sites_sub_page_title);
    }

    private View getBlockedSitesPageRootView() {
        return getRootViewSanitized(R.string.settings_fledge_page_blocked_sites_sub_page_title);
    }

    private View getLearnMoreRootView() {
        return getRootViewSanitized(R.string.settings_fledge_page_learn_more_heading);
    }

    private void setFledgePrefEnabled(boolean isEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        FledgeFragment.setFledgePrefEnabled(
                                ProfileManager.getLastUsedRegularProfile(), isEnabled));
    }

    private boolean isFledgePrefEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        FledgeFragment.isFledgePrefEnabled(
                                ProfileManager.getLastUsedRegularProfile()));
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
    @Feature({"RenderTest"})
    public void testRenderAllSitesPage() throws IOException {
        setFledgePrefEnabled(true);
        for (int i = 0; i < FledgeFragment.MAX_DISPLAYED_SITES + 1; i++) {
            mFakePrivacySandboxBridge.setFledgeJoiningAllowed(generateSiteFromNr(i), true);
        }
        startFledgeSettings();
        scrollToSetting(withText(R.string.settings_fledge_page_see_all_sites_label));
        onView(withText(R.string.settings_fledge_page_see_all_sites_label)).perform(click());
        mRenderTestRule.render(getAllSitesPageRootView(), "fledge_all_sites_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedSitesPageEmpty() throws IOException {
        setFledgePrefEnabled(false);
        startFledgeSettings();
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());
        mRenderTestRule.render(getBlockedSitesPageRootView(), "fledge_blocked_sites_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderBlockedSitesPagePopulated() throws IOException {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());
        mRenderTestRule.render(getBlockedSitesPageRootView(), "fledge_blocked_sites_populated");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderLearnMore() throws IOException {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();
        onView(withText(containsString("30 days. Learn more"))).perform(clickOnClickableSpan(0));
        mRenderTestRule.render(getLearnMoreRootView(), "fledge_learn_more");
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

        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Fledge.Enabled"));
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

        // Check that the sites list is displayed when Fledge is enabled.
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));

        // Check that actions are reported
        assertThat(
                mUserActionTester.getActions(), hasItems("Settings.PrivacySandbox.Fledge.Enabled"));
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

        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Fledge.Disabled"));
    }

    @Test
    @SmallTest
    public void testPopulateSitesList() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        // Check that the all sites pref is not displayed
        onView(withText(R.string.settings_fledge_page_see_all_sites_label)).check(doesNotExist());

        // Check that the sites are displayed.
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMaxDisplayedSites() {
        setFledgePrefEnabled(true);
        for (int i = 0; i < FledgeFragment.MAX_DISPLAYED_SITES + 1; i++) {
            mFakePrivacySandboxBridge.setFledgeJoiningAllowed(generateSiteFromNr(i), true);
        }
        startFledgeSettings();

        // Scroll to pref below last displayed site.
        scrollToSetting(withText(R.string.settings_fledge_page_see_all_sites_label));

        String lastDisplayedSite = generateSiteFromNr(FledgeFragment.MAX_DISPLAYED_SITES - 1);
        String firstNotDisplayedSite = generateSiteFromNr(FledgeFragment.MAX_DISPLAYED_SITES);

        // Verify that only MAX_DISPLAY_SITES are shown.
        onView(withText(lastDisplayedSite)).check(matches(isDisplayed()));
        onView(withText(firstNotDisplayedSite)).check(doesNotExist());

        // Navigate to All Sites page.
        onView(withText(R.string.settings_fledge_page_see_all_sites_label)).perform(click());

        // Verify that all sites are displayed
        scrollToSetting(withText(firstNotDisplayedSite));
        onView(withText(firstNotDisplayedSite)).check(matches(isDisplayed()));

        // Verify actions are reported
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Fledge.AllSitesOpened"));
    }

    @Test
    @SmallTest
    public void testBlockSites() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setCurrentFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        // Remove the first site from the list.
        clickImageButtonNextToText(SITE_NAME_1);
        onView(withText(SITE_NAME_1)).check(doesNotExist());
        onView(withText(R.string.settings_fledge_page_block_site_snackbar))
                .check(matches(isDisplayed()));

        // Remove the second site from the list.
        clickImageButtonNextToText(SITE_NAME_2);
        onView(withText(SITE_NAME_2)).check(doesNotExist());
        onView(withText(R.string.settings_fledge_page_block_site_snackbar))
                .check(matches(isDisplayed()));

        // Check that the empty state UI is displayed when the sites list is empty.
        onView(withText(R.string.settings_fledge_page_current_sites_description_empty))
                .check(matches(isDisplayed()));

        // Open the blocked sites sub-page.
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());
        onViewWaiting(withText(R.string.settings_fledge_page_blocked_sites_sub_page_title));

        // Verify that the sites are blocked.
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));

        // Verify that actions are reported
        assertThat(
                mUserActionTester.getActions(),
                hasItems(
                        "Settings.PrivacySandbox.Fledge.BlockedSitesOpened",
                        "Settings.PrivacySandbox.Fledge.SiteRemoved"));
    }

    @Test
    @SmallTest
    public void testUnblockSites() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();

        // Open the blocked sites sub-page.
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());
        onViewWaiting(withText(R.string.settings_fledge_page_blocked_sites_sub_page_title));

        // Unblock the first site.
        clickImageButtonNextToText(SITE_NAME_1);
        onView(withText(SITE_NAME_1)).check(doesNotExist());
        onView(withText(R.string.settings_fledge_page_add_site_snackbar))
                .check(matches(isDisplayed()));

        // Unblock the second site.
        clickImageButtonNextToText(SITE_NAME_2);
        onView(withText(SITE_NAME_2)).check(doesNotExist());
        onView(withText(R.string.settings_fledge_page_add_site_snackbar))
                .check(matches(isDisplayed()));

        // Check that the empty state UI is displayed when the site list is empty.
        onView(withText(R.string.settings_fledge_page_blocked_sites_description_empty))
                .check(matches(isDisplayed()));

        // Go back to the main Fledge fragment.
        pressBack();
        onViewWaiting(withText(R.string.settings_fledge_page_toggle_sub_label));

        // Verify that the sites are unblocked.
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));

        // Verify that actions are reported
        assertThat(
                mUserActionTester.getActions(),
                hasItems(
                        "Settings.PrivacySandbox.Fledge.BlockedSitesOpened",
                        "Settings.PrivacySandbox.Fledge.SiteAdded"));
    }

    @Test
    @SmallTest
    public void testBlockedSitesAppearWhenFledgeOff() {
        setFledgePrefEnabled(false);
        mFakePrivacySandboxBridge.setBlockedFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());

        onViewWaiting(withText(R.string.settings_fledge_page_blocked_sites_sub_page_title));
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));

        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Fledge.BlockedSitesOpened"));
    }

    @Test
    @SmallTest
    public void testBlockedSitesAppearWhenFledgeOn() {
        setFledgePrefEnabled(true);
        mFakePrivacySandboxBridge.setBlockedFledgeSites(SITE_NAME_1, SITE_NAME_2);
        startFledgeSettings();
        onView(withText(R.string.settings_fledge_page_blocked_sites_heading)).perform(click());

        onViewWaiting(withText(R.string.settings_fledge_page_blocked_sites_sub_page_title));
        onView(withText(SITE_NAME_1)).check(matches(isDisplayed()));
        onView(withText(SITE_NAME_2)).check(matches(isDisplayed()));

        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Fledge.BlockedSitesOpened"));
    }

    @Test
    @SmallTest
    @Policies.Add({
        @Policies.Item(key = "PrivacySandboxSiteEnabledAdsEnabled", string = "false"),
        @Policies.Item(key = "PrivacySandboxPromptEnabled", string = "false")
    })
    public void testFledgeManaged() {
        startFledgeSettings();

        // Check default state and try to press the toggle.
        assertFalse(isFledgePrefEnabled());
        onView(getFledgeToggleMatcher()).check(matches(not(isChecked())));
        onView(getFledgeToggleMatcher()).perform(click());

        // Check that the state of the pref and the toggle did not change.
        assertFalse(isFledgePrefEnabled());
        onView(getFledgeToggleMatcher()).check(matches(not(isChecked())));
    }

    @Test
    @SmallTest
    public void testLearnMoreLink() {
        startFledgeSettings();
        // Open the Fledge learn more activity
        onView(withText(containsString("30 days. Learn more"))).perform(clickOnClickableSpan(0));
        onViewWaiting(withText(R.string.settings_fledge_page_learn_more_heading))
                .check(matches(isDisplayed()));
        // Close the additional activity
        pressBack();
        assertThat(
                mUserActionTester.getActions(),
                hasItems("Settings.PrivacySandbox.Fledge.LearnMoreClicked"));
    }

    @Test
    @SmallTest
    public void testFooterTopicsLink() throws IOException {
        setFledgePrefEnabled(true);
        startFledgeSettings();
        // Open a Topics settings activity.
        onView(withText(containsString("ad topics"))).perform(clickOnClickableSpan(0));
        onViewWaiting(withText(R.string.settings_topics_page_toggle_sub_label_v2))
                .check(matches(isDisplayed()));
        // Close the additional activity by navigating back.
        pressBack();
    }

    @Test
    @SmallTest
    public void testFooterCookieSettingsLink() throws IOException {
        setFledgePrefEnabled(true);
        startFledgeSettings();
        // Open a CookieSettings activity.
        onView(withText(containsString("cookie settings"))).perform(clickOnClickableSpan(1));
        onViewWaiting(withText(R.string.third_party_cookies_page_title))
                .check(matches(isDisplayed()));
        // Close the additional activity by navigating back.
        pressBack();
    }
}
