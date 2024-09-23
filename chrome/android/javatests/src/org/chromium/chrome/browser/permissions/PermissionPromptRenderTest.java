// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
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
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";
    private static final int TEST_PORT = 12345;

    private boolean mNightModeEnabled;
    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_MESSAGES)
                    .build();

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
    @Features.DisableFeatures(PermissionsAndroidFeatureList.ONE_TIME_PERMISSION)
    public void testGeolocationRegularPrompt() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        mPermissionRule.loadUrl(mPermissionRule.getURL(TEST_FILE));

        testPrompt(/* goldenViewId= */ "regularPrompt");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @Features.EnableFeatures(PermissionsAndroidFeatureList.ONE_TIME_PERMISSION)
    public void testGeolocationOneTimePrompt() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePrompt");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @CommandLineFlags.Add({
        "enable-features=" + PermissionsAndroidFeatureList.ONE_TIME_PERMISSION + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:show_allow_always_as_first_button/true"
    })
    public void testGeolocationOneTimePromptWithAllowAlwaysFirst() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePromptAllowAlwaysAsFirstButton");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @CommandLineFlags.Add({
        "enable-features=" + PermissionsAndroidFeatureList.ONE_TIME_PERMISSION + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:show_allow_always_as_first_button/true/"
                + "use_stronger_prompt_language/true/use_while_visiting_language/true"
    })
    public void testGeolocationOneTimePromptWithAllowWhileVisitingFirst() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        mPermissionRule.setUpUrl(TEST_FILE);
        testPrompt(/* goldenViewId= */ "oneTimePromptAllowWhileVisitingAsFirstButton");
    }

    @Test
    @MediumTest
    @Feature({"Prompt", "RenderTest"})
    @Features.EnableFeatures(PermissionsAndroidFeatureList.ONE_TIME_PERMISSION)
    public void testGeolocationOneTimePromptLongOriginWrapsToNextLineAndIsNotElided()
            throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        mPermissionRule.setupUrlWithHostName(
                "unelided.long.wrapping.hostname.with.subdomains.com", TEST_FILE);

        testPrompt(/* goldenViewId= */ "oneTimePromptLongOrigin");
    }
}
