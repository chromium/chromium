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

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
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
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

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

    @Rule public MockitoRule mockito = MockitoJUnit.rule();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mActionTester;
    @Mock private SettingsNavigation mSettingsNavigation;

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
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.PRIVACY_GUIDE_VIEWED, isViewed));
    }

    private boolean isPrivacyGuideViewed() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .getBoolean(Pref.PRIVACY_GUIDE_VIEWED));
    }

    private void setShowTrackingProtection(boolean show) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED, show));
    }

    private void setIpProtection(boolean ipProtectionEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.IP_PROTECTION_ENABLED, ipProtectionEnabled));
    }

    private boolean isIpProtectionEnabled() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .getBoolean(Pref.IP_PROTECTION_ENABLED));
    }

    private void setFpProtection(boolean fpProtectionEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(
                                        Pref.FINGERPRINTING_PROTECTION_ENABLED,
                                        fpProtectionEnabled));
    }

    private boolean isFpProtectionEnabled() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .getBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED));
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
        mActionTester = new UserActionTester();
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
        ThreadUtils.runOnUiThreadBlocking(
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
    @Features.EnableFeatures(ChromeFeatureList.IP_PROTECTION_UX)
    public void testIpProtectionFragment() throws IOException {
        setShowTrackingProtection(false);
        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ip_protection_title));
        onView(withText(R.string.ip_protection_title)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.ip_protection_title)).check(matches(isDisplayed()));
        // Verify that the user action is emitted when ip protection is clicked
        assertTrue(
                mActionTester.getActions().contains("Settings.IpProtection.OpenedFromPrivacyPage"));
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(PRIVACY_SANDBOX_V4_POS_IDX);
                });
        onView(withText(R.string.ad_privacy_link_row_label)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD_UX)
    public void testTrackingProtection() throws IOException {
        setShowTrackingProtection(true);
        mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Tracking Protection row is shown and 3PC/DNT are not.
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference trackingProtectionPreference =
                fragment.findPreference(PrivacySettings.PREF_TRACKING_PROTECTION);
        assertTrue(trackingProtectionPreference.isVisible());
        onView(withText(R.string.tracking_protection_title)).check(matches(isDisplayed()));

        Preference thirdPartyCookiesPreference =
                fragment.findPreference(PrivacySettings.PREF_THIRD_PARTY_COOKIES);
        assertFalse(thirdPartyCookiesPreference.isVisible());

        Preference dntPreference = fragment.findPreference(PrivacySettings.PREF_DO_NOT_TRACK);
        assertFalse(dntPreference.isVisible());
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD_UX)
    public void testTrackingProtectionRewind() throws IOException {
        setShowTrackingProtection(true);
        mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the 3PC and DNT rows are shown instead of Tracking Protection.
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference trackingProtectionPreference =
                fragment.findPreference(PrivacySettings.PREF_TRACKING_PROTECTION);
        assertTrue(trackingProtectionPreference.isVisible());
        onView(withText(R.string.third_party_cookies_link_row_label)).check(matches(isDisplayed()));

        Preference dntPreference = fragment.findPreference(PrivacySettings.PREF_DO_NOT_TRACK);
        assertTrue(dntPreference.isVisible());

        Preference thirdPartyCookiesPreference =
                fragment.findPreference(PrivacySettings.PREF_THIRD_PARTY_COOKIES);
        assertFalse(thirdPartyCookiesPreference.isVisible());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.IP_PROTECTION_UX)
    public void testIpProtectionSettingsE2E() throws ExecutionException {
        setIpProtection(false);
        setShowTrackingProtection(false);
        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ip_protection_title));
        onView(withText(R.string.ip_protection_title)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.ip_protection_title)).check(matches(isDisplayed()));
        onView(allOf(withText(R.string.text_off), isDisplayed())).perform(click());
        assertTrue(isIpProtectionEnabled());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.FINGERPRINTING_PROTECTION_UX)
    public void testFingerprintingProtectionSettingsE2E() throws ExecutionException {
        setFpProtection(false);
        setShowTrackingProtection(false);
        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.tracking_protection_fingerprinting_protection_title));
        onView(withText(R.string.tracking_protection_fingerprinting_protection_title))
                .perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.tracking_protection_fingerprinting_protection_title))
                .check(matches(isDisplayed()));
        onView(allOf(withText(R.string.text_off), isDisplayed())).perform(click());
        assertTrue(isFpProtectionEnabled());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.IP_PROTECTION_UX)
    public void testIpProtectionSettingsWithTrackingProtectionEnabled() {
        setIpProtection(false);
        setShowTrackingProtection(true);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();

        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference ipProtectionPreference =
                fragment.findPreference(PrivacySettings.PREF_IP_PROTECTION);

        assertFalse(ipProtectionPreference.isVisible());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.FINGERPRINTING_PROTECTION_UX)
    public void testFingerprintingProtectionSettingsWithTrackingProtectionEnabled() {
        setFpProtection(false);
        setShowTrackingProtection(true);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();

        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference fpProtectionPreference =
                fragment.findPreference(PrivacySettings.PREF_FP_PROTECTION);

        assertFalse(fpProtectionPreference.isVisible());
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
    public void testPrivacyGuideLinkRowEntryPointUserAction() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        // Scroll down and open Privacy Guide page.
        scrollToSetting(withText(R.string.privacy_guide_pref_summary));
        onView(withText(R.string.privacy_guide_pref_summary)).perform(click());
        // Verify that the user action is emitted when privacy guide is clicked
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.StartPrivacySettings"));
    }

    @Test
    @LargeTest
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
    // A random policy is required to make the device managed
    @Policies.Add({@Policies.Item(key = "RandomPolicy", string = "true")})
    public void testPrivacyGuideNotDisplayedWhenDeviceIsManaged() {
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.privacy_guide_pref_summary)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/1437093")
    public void testPrivacyGuideNotDisplayedWhenUserIsChild() {
        // TODO(crbug.com/40264499): Remove once SigninChecker is automatically created.
        ThreadUtils.runOnUiThreadBlocking(
                () -> SigninCheckerProvider.get(ProfileManager.getLastUsedRegularProfile()));
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.privacy_guide_pref_summary)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP)
    @Features.EnableFeatures(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID)
    public void testClearBrowsingData_withQuickDeleteV2Disabled() {
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.clear_browsing_data_title)).check(matches(isDisplayed()));

        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference ClearBrowsingDataPreference =
                fragment.findPreference(PrivacySettings.PREF_CLEAR_BROWSING_DATA);
        Preference ClearBrowsingDataAdvancedPreference =
                fragment.findPreference(PrivacySettings.PREF_CLEAR_BROWSING_DATA_ADVANCED);
        assertTrue(ClearBrowsingDataPreference.isVisible());
        assertFalse(ClearBrowsingDataAdvancedPreference.isVisible());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({
        ChromeFeatureList.QUICK_DELETE_FOR_ANDROID,
        ChromeFeatureList.QUICK_DELETE_ANDROID_FOLLOWUP
    })
    public void testClearBrowsingData_withQuickDeleteV2Enabled() {
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.clear_browsing_data_title)).check(matches(isDisplayed()));

        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        Preference ClearBrowsingDataPreference =
                fragment.findPreference(PrivacySettings.PREF_CLEAR_BROWSING_DATA);
        Preference ClearBrowsingDataAdvancedPreference =
                fragment.findPreference(PrivacySettings.PREF_CLEAR_BROWSING_DATA_ADVANCED);
        assertTrue(ClearBrowsingDataAdvancedPreference.isVisible());
        assertFalse(ClearBrowsingDataPreference.isVisible());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignedOutFooterLink() {
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        String footer =
                mSettingsActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.privacy_chrome_data_and_google_services_signed_out_footer);
        String footerWithoutSpans =
                SpanApplier.applySpans(footer, new SpanInfo("<link>", "</link>", new Object()))
                        .toString();
        onView(withText(containsString(footerWithoutSpans))).perform(clickOnClickableSpan(0));

        verify(mSettingsNavigation).startSettings(any(), eq(GoogleServicesSettings.class));
    }

    @Test
    @LargeTest
    public void testSettingsFragmentAttachedMetric() {
        // Expect "PrivacySettings".hashCode() to be logged.
        int expectedValue = 1505293227;
        assertEquals(expectedValue, "PrivacySettings".hashCode());
        try (var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Settings.FragmentAttached", expectedValue)) {
            mSettingsActivityTestRule.startSettingsActivity();
            SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        }
    }
}
