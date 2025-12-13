// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeoutException;

@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
public class LocationPrecisionChooserRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";
    private static final int TEST_PORT = 12345;

    private final boolean mNightModeEnabled;
    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_MESSAGES)
                    .build();

    /**
     * An activity that we can override configuration on it's creation. We need this for some tests
     * that require specific setups, like tests with small screens.
     */
    public static class PermissionTestActivity extends ChromeTabbedActivity {
        @Override
        @SuppressLint("CheckResult")
        protected boolean applyOverrides(Context baseContext, Configuration overrideConfig) {
            super.applyOverrides(baseContext, overrideConfig);
            overrideConfig.densityDpi = 1300;
            overrideConfig.screenWidthDp = 80;
            overrideConfig.screenHeightDp = 80;
            return true;
        }
    }

    public LocationPrecisionChooserRenderTest(boolean nightModeEnabled) {
        mNightModeEnabled = nightModeEnabled;
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mPermissionRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mPermissionRule.getEmbeddedTestServerRule().setServerPort(TEST_PORT);
    }

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(mNightModeEnabled);
    }

    @After
    public void tearDown() throws TimeoutException {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private void testPrompt(String goldenViewId) throws TimeoutException, IOException {
        mPermissionRule.runJavaScriptCodeWithUserGestureInCurrentTab(
                "initiate_getCurrentPosition()");

        mPermissionRule.waitForDialogShownState(true);

        View modalDialogView = mPermissionRule.getActivity().findViewById(R.id.modal_dialog_view);
        RenderTestRule.sanitize(modalDialogView);

        mRenderTestRule.render(modalDialogView, goldenViewId);
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/1")
    public void testGeolocationOneTimePrompt1() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm1");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/2")
    public void testGeolocationOneTimePromptArm2() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm2");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/3")
    public void testGeolocationOneTimePromptArm3() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm3");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/4")
    public void testGeolocationOneTimePromptArm4() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm4");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/5")
    public void testGeolocationOneTimePromptArm5() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm5");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission:prompt_arm/6")
    public void testGeolocationOneTimePromptArm6() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt_location_arm6");
    }
}
