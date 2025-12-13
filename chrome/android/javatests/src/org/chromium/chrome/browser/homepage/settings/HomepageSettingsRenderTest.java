// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for {@link HomepageSettings}.
 *
 * <p>Also serves as a render test for {@link ChromeSwitchPreference} and {@link
 * RadioButtonGroupHomepagePreference}
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class HomepageSettingsRenderTest {

    private static final String TEST_URL = JUnitTestGURLs.URL_1.getSpec();
    private static final GURL TEST_GURL = JUnitTestGURLs.URL_1;
    private static final GURL NTP_GURL = JUnitTestGURLs.NTP_URL;

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2) // Revision incremented for new tests
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HomepagePolicyManager mMockHomepagePolicyManager;
    @Mock private PartnerBrowserCustomizations mMockPartnerBrowserCustomizations;
    @Mock private Profile mProfile;

    private final boolean mNightModeEnabled;

    public HomepageSettingsRenderTest(boolean nightModeEnabled) {
        mNightModeEnabled = nightModeEnabled;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        // Mock necessary singletons.
        HomepagePolicyManager.setInstanceForTests(mMockHomepagePolicyManager);
        PartnerBrowserCustomizations.setInstanceForTesting(mMockPartnerBrowserCustomizations);

        mSettingsRule.launchActivity(null);
        Activity mActivity = mSettingsRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private void launchFragmentAndRender(String renderId) throws IOException {
        mSettingsRule.launchPreference(
                HomepageSettings.class,
                null,
                fragment -> ((HomepageSettings) fragment).setProfile(mProfile));

        View view = mSettingsRule.getPreferenceFragment().getView();
        // Set a fake background color to make the screenshots easier to compare with bare eyes.
        int backgroundColor = mNightModeEnabled ? Color.BLACK : Color.WHITE;
        ThreadUtils.runOnUiThreadBlocking(() -> view.setBackgroundColor(backgroundColor));

        // Wait for the view to be laid out.
        CriteriaHelper.pollUiThread(
                () -> view.isShown() && view.getWidth() > 0 && view.getHeight() > 0,
                "View not rendered.");

        // Sanitize views to disable blinking cursors, etc.
        RenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "homepage_settings" + renderId);
    }

    // Helper functions for setting up mock states.

    private void setupShowHomeButtonPolicy(boolean enabled) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isShowHomeButtonPolicyManaged();
        Mockito.doReturn(enabled).when(mMockHomepagePolicyManager).getShowHomeButtonPolicyValue();
    }

    private void setupHomepageLocationPolicy(GURL url) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isHomepageLocationPolicyManaged();
        Mockito.doReturn(url).when(mMockHomepagePolicyManager).getHomepageLocationPolicyUrl();
    }

    private void setupHomepageIsNtpPolicy(boolean isNtp) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isHomepageIsNtpPolicyManaged();
        Mockito.doReturn(isNtp).when(mMockHomepagePolicyManager).getHomepageIsNtpPolicyValue();
    }

    private void setupShowHomeButtonRecommendation(boolean isFollowing) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isShowHomeButtonPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageButtonPolicyRecommendation();
    }

    private void setupHomepageIsNtpRecommendation(boolean recommendNtp, boolean isFollowing) {
        Mockito.doReturn(true)
                .when(mMockHomepagePolicyManager)
                .isHomepageSelectionPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageSelectionPolicyRecommendation();
        // To properly simulate the recommendation, we need HomepageManager to return the
        // correct value. We'll use the test rule to set the underlying preference.
        if (recommendNtp) {
            ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        } else {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        }
    }

    private void setupHomepageLocationRecommendation(GURL url, boolean isFollowing) {
        Mockito.doReturn(true)
                .when(mMockHomepagePolicyManager)
                .isHomepageSelectionPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageSelectionPolicyRecommendation();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(url.getSpec()));
    }

    private void setupPartnerHomepage(GURL url) {
        Mockito.doReturn(true)
                .when(mMockPartnerBrowserCustomizations)
                .isHomepageProviderAvailableAndEnabled();
        Mockito.doReturn(url).when(mMockPartnerBrowserCustomizations).getHomePageUrl();
    }

    // Unmanaged States
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageOnNtp() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_unmanaged_homepage_on_ntp");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageOnCustom() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_unmanaged_homepage_on_custom");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageOff() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL);
                    mHomepageTestRule.disableHomepageForTest();
                });
        launchFragmentAndRender("_unmanaged_homepage_off");
    }

    // Managed States
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyShowHomeButtonOff() throws IOException {
        setupShowHomeButtonPolicy(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageTestRule.useChromeNtpForTest();
                    mHomepageTestRule.disableHomepageForTest();
                });
        launchFragmentAndRender("_policy_show_home_button_off");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyShowHomeButtonOn() throws IOException {
        setupShowHomeButtonPolicy(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_policy_show_home_button_on");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyLocationCustom() throws IOException {
        setupHomepageLocationPolicy(TEST_GURL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_policy_location_custom");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyLocationNtp() throws IOException {
        setupHomepageLocationPolicy(NTP_GURL);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_policy_location_ntp");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyHomepageIsNtpOn() throws IOException {
        setupHomepageIsNtpPolicy(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_policy_homepage_is_ntp_on");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyHomepageIsNtpOff() throws IOException {
        setupHomepageIsNtpPolicy(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_policy_homepage_is_ntp_off");
    }

    // Recommended States: ShowHomeButton
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonRecommended_Following() throws IOException {
        setupShowHomeButtonRecommendation(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_recommended_show_home_button_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonRecommended_Overridden() throws IOException {
        setupShowHomeButtonRecommendation(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageTestRule.useChromeNtpForTest();
                    mHomepageTestRule.disableHomepageForTest(); // Simulate user override.
                });
        launchFragmentAndRender("_recommended_show_home_button_overridden");
    }

    // Recommended States: HomepageIsNTP
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageIsNtpRecommended_On_Following() throws IOException {
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ true);
        launchFragmentAndRender("_recommended_ntp_on_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageIsNtpRecommended_On_Overridden() throws IOException {
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ false);
        // The helper sets NTP, but we override to custom to create the "overridden" state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_recommended_ntp_on_overridden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageIsNtpRecommended_Off_Following() throws IOException {
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ true);
        launchFragmentAndRender("_recommended_ntp_off_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageIsNtpRecommended_Off_Overridden() throws IOException {
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ false);
        // The helper sets a custom URL, but we override to NTP to create the "overridden" state.
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_recommended_ntp_off_overridden");
    }

    // Recommended States: HomepageLocation
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageLocationRecommended_Following() throws IOException {
        setupHomepageLocationRecommendation(TEST_GURL, /* isFollowing= */ true);
        launchFragmentAndRender("_recommended_location_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageLocationRecommended_Overridden() throws IOException {
        setupHomepageLocationRecommendation(TEST_GURL, /* isFollowing= */ false);
        // The helper sets the custom URL, but we override to NTP.
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_recommended_location_overridden");
    }

    // Combined Managed and Recommended States
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOn_HomepageIsNtpRecommendedOn_Following()
            throws IOException {
        setupShowHomeButtonPolicy(true);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ true);
        launchFragmentAndRender("_managed_on_recommended_ntp_on_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOn_HomepageIsNtpRecommendedOn_Overridden()
            throws IOException {
        setupShowHomeButtonPolicy(true);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_managed_on_recommended_ntp_on_overridden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOn_HomepageIsNtpRecommendedOff_Following()
            throws IOException {
        setupShowHomeButtonPolicy(true);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ true);
        launchFragmentAndRender("_managed_on_recommended_ntp_off_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOn_HomepageIsNtpRecommendedOff_Overridden()
            throws IOException {
        setupShowHomeButtonPolicy(true);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ false);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_managed_on_recommended_ntp_off_overridden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOff_HomepageIsNtpRecommendedOn_Following()
            throws IOException {
        setupShowHomeButtonPolicy(false);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ true);
        launchFragmentAndRender("_managed_off_recommended_ntp_on_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOff_HomepageIsNtpRecommendedOn_Overridden()
            throws IOException {
        setupShowHomeButtonPolicy(false);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ true, /* isFollowing= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("_managed_off_recommended_ntp_on_overridden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOff_HomepageIsNtpRecommendedOff_Following()
            throws IOException {
        setupShowHomeButtonPolicy(false);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ true);
        launchFragmentAndRender("_managed_off_recommended_ntp_off_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ShowHomeButtonManagedOff_HomepageIsNtpRecommendedOff_Overridden()
            throws IOException {
        setupShowHomeButtonPolicy(false);
        setupHomepageIsNtpRecommendation(/* recommendNtp= */ false, /* isFollowing= */ false);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("_managed_off_recommended_ntp_off_overridden");
    }

    // Partner Homepage State
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PartnerHomepage() throws IOException {
        setupPartnerHomepage(TEST_GURL);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useDefaultHomepageForTest());
        launchFragmentAndRender("_partner_homepage");
    }
}
