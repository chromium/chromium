// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;

import androidx.annotation.CallSuper;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
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
public class PermissionPromptRenderTest {
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
        @CallSuper
        protected boolean applyOverrides(Context baseContext, Configuration overrideConfig) {
            super.applyOverrides(baseContext, overrideConfig);
            overrideConfig.densityDpi = 1300;
            overrideConfig.screenWidthDp = 80;
            overrideConfig.screenHeightDp = 80;
            return true;
        }
    }

    public PermissionPromptRenderTest(boolean nightModeEnabled) {
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

        mRenderTestRule.render(
                mPermissionRule.getActivity().findViewById(R.id.modal_dialog_view), goldenViewId);
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    public void testGeolocationOneTimePrompt() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testGeolocationOneTimePromptWithRadioButtons() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimeWithRadioButtonsPrompt");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    public void testGeolocationOneTimePromptWithAllowAlwaysFirst() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePromptAllowAlwaysAsFirstButton");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    public void testGeolocationOneTimePromptWithAllowWhileVisitingFirst() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePromptAllowWhileVisitingAsFirstButton");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    public void testGeolocationOneTimePromptLongOriginWrapsToNextLineAndIsNotElided()
            throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        mPermissionRule.setupUrlWithHostName(
                "unelided.long.wrapping.hostname.with.subdomains.com", TEST_FILE);

        testPrompt(/* goldenViewId= */ "oneTimePromptLongOrigin");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @DisabledTest(message = "crbug.com/385114151")
    public void testNegativeButtonOutOfScreen() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(ContextUtils.getApplicationContext(), PermissionTestActivity.class);
        intent.setData(Uri.parse("about:blank"));
        mPermissionRule.setActivity(
                ApplicationTestUtils.waitForActivityWithClass(
                        PermissionTestActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent)));
        mPermissionRule.setUpUrl(TEST_FILE);
        var histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Permissions.OneTimePermission.Android.NegativeButtonOutOfScreen",
                                true)
                        .build();

        testPrompt(/* goldenViewId= */ "oneTimePromptNegativeButtonOutOfScreen");
        histogramExpectation.assertExpected("Should record negative button out of screen");
    }
}
