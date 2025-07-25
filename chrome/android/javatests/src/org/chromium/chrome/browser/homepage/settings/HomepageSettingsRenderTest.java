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
                    .setRevision(1)
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

    private void setupRecommendedOnPolicy(boolean isFollowing) {
        Mockito.doReturn(true).when(mMockHomepagePolicyManager).isShowHomeButtonPolicyRecommended();
        Mockito.doReturn(isFollowing)
                .when(mMockHomepagePolicyManager)
                .isFollowingHomepageButtonPolicyRecommendation();
    }

    private void setupPartnerHomepage(GURL url) {
        Mockito.doReturn(true)
                .when(mMockPartnerBrowserCustomizations)
                .isHomepageProviderAvailableAndEnabled();
        Mockito.doReturn(url).when(mMockPartnerBrowserCustomizations).getHomePageUrl();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageOnNtp() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("homepage_on_ntp");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_HomepageOnCustom() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("homepage_on_custom");
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
        launchFragmentAndRender("homepage_off");
    }

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
        launchFragmentAndRender("policy_show_home_button_off");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyShowHomeButtonOn() throws IOException {
        setupShowHomeButtonPolicy(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("policy_show_home_button_on");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyLocationCustom() throws IOException {
        setupHomepageLocationPolicy(TEST_GURL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("policy_location_custom");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyLocationNtp() throws IOException {
        setupHomepageLocationPolicy(NTP_GURL);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("policy_location_ntp");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyHomepageIsNtpOn() throws IOException {
        setupHomepageIsNtpPolicy(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("policy_homepage_is_ntp_on");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PolicyHomepageIsNtpOff() throws IOException {
        setupHomepageIsNtpPolicy(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL));
        launchFragmentAndRender("policy_homepage_is_ntp_off");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_RecommendedOn_Following() throws IOException {
        setupRecommendedOnPolicy(true);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useChromeNtpForTest());
        launchFragmentAndRender("recommended_on_following");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_RecommendedOn_Overridden() throws IOException {
        setupRecommendedOnPolicy(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageTestRule.useChromeNtpForTest();
                    // Simulate user override.
                    mHomepageTestRule.disableHomepageForTest();
                });
        launchFragmentAndRender("recommended_on_overridden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PartnerHomepage() throws IOException {
        setupPartnerHomepage(TEST_GURL);
        ThreadUtils.runOnUiThreadBlocking(() -> mHomepageTestRule.useDefaultHomepageForTest());
        launchFragmentAndRender("partner_homepage");
    }
}
