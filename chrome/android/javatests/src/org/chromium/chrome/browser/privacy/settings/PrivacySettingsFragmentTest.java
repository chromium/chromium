// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideInteractions;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Tests for {@link PrivacySettings}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Child account can leak to other tests in the suite.")
// Disable TrackingProtection3pcd as we use prefs instead of the feature in these tests.
@DisableFeatures({ChromeFeatureList.TRACKING_PROTECTION_3PCD})
public class PrivacySettingsFragmentTest {
    // Index of the Privacy Sandbox row entry in the settings list.
    public static final int PRIVACY_SANDBOX_V4_POS_IDX = 4;
    // Name of the histogram to record the entry on Privacy Guide via the S&P link-row.
    public static final String ENTRY_EXIT_HISTOGRAM = "Settings.PrivacyGuide.EntryExit";

    public final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mSettingsActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mActionTester;

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.menu_id_targeted_help)
                            != null;
                });
    }

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private View getIncognitoReauthSettingView(PrivacySettings privacySettings) {
        String incognito_lock_title =
                mSettingsActivityTestRule
                        .getActivity()
                        .getString(R.string.settings_incognito_tab_lock_title);
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(withText(incognito_lock_title))));
        onView(withText(incognito_lock_title)).check(matches(isDisplayed()));
        for (int i = 0; i < privacySettings.getListView().getChildCount(); ++i) {
            View view = privacySettings.getListView().getChildAt(i);
            TextView titleView = view.findViewById(android.R.id.title);
            if (titleView != null) {
                String title = titleView.getText().toString();
                if (TextUtils.equals(incognito_lock_title, title)) {
                    return view;
                }
            }
        }
        return null;
    }

    private void setPrivacyGuideViewed(boolean isViewed) {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .setBoolean(Pref.PRIVACY_GUIDE_VIEWED, isViewed));
    }

    private boolean isPrivacyGuideViewed() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .getBoolean(Pref.PRIVACY_GUIDE_VIEWED));
    }

    private void setShowTrackingProtection(boolean show) {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(Profile.getLastUsedRegularProfile())
                                .setBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED, show));
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

    @After
    public void tearDown() {
        if (mActionTester != null) mActionTester.tearDown();
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderTopView() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_settings_top_view");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderBottomView() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_settings_bottom_view");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testRenderWhenPrivacyGuideViewed() throws IOException {
        setPrivacyGuideViewed(true);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_privacy_guide_label_without_new");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testRenderWhenPrivacyGuideNotViewed() throws IOException {
        setPrivacyGuideViewed(false);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_privacy_guide_label_with_new");
    }

    @Test
    @LargeTest
    public void testPrivacySandboxV4View() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ad_privacy_link_row_label));
        onView(withText(R.string.ad_privacy_link_row_label)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testPrivacySandboxV4RestrictedWithRestrictedNoticeEnabled() throws IOException {
        mFakePrivacySandboxBridge.setRestrictedNoticeEnabled(true);
        mFakePrivacySandboxBridge.setPrivacySandboxRestricted(true);

        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ad_privacy_link_row_label));
        // Verify that the right subtitle is shown.
        onView(withText(R.string.settings_ad_privacy_restricted_link_row_sub_label))
                .check(matches(isDisplayed()));
        onView(withText(R.string.ad_privacy_link_row_label)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.settings_ad_measurement_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testPrivacySandboxV4NotRestrictedWithRestrictedNoticeEnabled() throws IOException {
        mFakePrivacySandboxBridge.setRestrictedNoticeEnabled(true);
        mFakePrivacySandboxBridge.setPrivacySandboxRestricted(false);

        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ad_privacy_link_row_label));
        // Verify that the right subtitle is shown.
        onView(withText(R.string.ad_privacy_link_row_sub_label)).check(matches(isDisplayed()));
        onView(withText(R.string.ad_privacy_link_row_label)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.settings_ad_measurement_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testPrivacySandboxV4ViewRestricted() throws IOException {
        mFakePrivacySandboxBridge.setPrivacySandboxRestricted(true);
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and verify that the Privacy Sandbox is not there.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(PRIVACY_SANDBOX_V4_POS_IDX);
                });
        onView(withText(R.string.ad_privacy_link_row_label)).check(doesNotExist());
    }

    @Test
    @LargeTest
    public void testTrackingProtectionWithSandboxV4() throws IOException {
        setShowTrackingProtection(true);
        mSettingsActivityTestRule.startSettingsActivity();
        // Verify that the Tracking Protection row is shown and 3PC/DNT is not.
        onView(withText(R.string.tracking_protection_title)).check(matches(isDisplayed()));
        onView(withText(R.string.third_party_cookies_link_row_label)).check(doesNotExist());
        onView(withText(R.string.do_not_track_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderIncognitoLockView_DeviceScreenLockDisabled() throws IOException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        mRenderTestRule.render(
                getIncognitoReauthSettingView(fragment),
                "incognito_reauth_setting_screen_lock_disabled");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderIncognitoLockView_DeviceScreenLockEnabled() throws IOException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        mRenderTestRule.render(
                getIncognitoReauthSettingView(fragment),
                "incognito_reauth_setting_screen_lock_enabled");
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testPrivacyGuideLinkRowEntryPointUserAction() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        mActionTester = new UserActionTester();
        // Scroll down and open Privacy Guide page.
        scrollToSetting(withText(R.string.privacy_guide_pref_summary));
        onView(withText(R.string.privacy_guide_pref_summary)).perform(click());
        // Verify that the user action is emitted when privacy guide is clicked
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.StartPrivacySettings"));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testPrivacyGuideLinkRowEntryExitHistogram() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY);

        // Scroll down and open Privacy Guide page.
        scrollToSetting(withText(R.string.privacy_guide_pref_summary));
        onView(withText(R.string.privacy_guide_pref_summary)).perform(click());

        histogram.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testPrivacyGuideNewLabelVisibility() throws ExecutionException {
        setPrivacyGuideViewed(false);
        mSettingsActivityTestRule.startSettingsActivity();
        assertFalse(isPrivacyGuideViewed());

        // Open the privacy guide
        onView(withText(R.string.privacy_guide_pref_summary)).perform(click());
        // Tapping on the privacy guide row should mark the privacy guide as viewed
        assertTrue(isPrivacyGuideViewed());
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    // A random policy is required to make the device managed
    @Policies.Add({@Policies.Item(key = "RandomPolicy", string = "true")})
    public void testPrivacyGuideNotDisplayedWhenDeviceIsManaged() {
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.privacy_guide_pref_summary)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    @DisabledTest(message = "crbug.com/1437093")
    public void testPrivacyGuideNotDisplayedWhenUserIsChild() {
        // TODO(crbug.com/1433652): Remove once SigninChecker is automatically created.
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> SigninCheckerProvider.get(Profile.getLastUsedRegularProfile()));
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.privacy_guide_pref_summary)).check(doesNotExist());
    }
}
