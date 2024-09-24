// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.ExecutionException;

/** Tests for {@link SafetyHubHatsBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB_ANDROID_SURVEY)
public class SafetyHubHatsBridgeTest {
    private static final String TEST_URL1 = "https://www.example.com/";
    private static final String TEST_URL2 = "https://www.google.com/";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private SafetyHubHatsBridge.Natives mSafetyHubHatsBridgeNatives;

    private TabModelSelector mTabModelSelector;
    private Profile mProfile;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws ExecutionException {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);

        mJniMocker.mock(SafetyHubHatsBridgeJni.TEST_HOOKS, mSafetyHubHatsBridgeNatives);
        doReturn(true)
                .when(mSafetyHubHatsBridgeNatives)
                .triggerHatsSurveyIfEnabled(any(), any(), any());

        mActivityTestRule.startMainActivityOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mProfile = ProfileManager.getLastUsedRegularProfile());
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @SmallTest
    public void testTriggerControlHatsSurvey() {
        SafetyHubHatsBridge safetyHubHatsBridge = new SafetyHubHatsBridge(mProfile);
        safetyHubHatsBridge.triggerControlHatsSurvey(mTabModelSelector);

        verifyHatsTrigger(SafetyHubHatsBridge.CONTROL_NOTIFICATION_MODULE);
    }

    @Test
    @SmallTest
    public void testTriggerProactiveHatsSurvey() {
        SafetyHubHatsBridge safetyHubHatsBridge = new SafetyHubHatsBridge(mProfile);
        safetyHubHatsBridge.triggerProactiveHatsSurvey(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);

        verifyHatsTrigger(MagicStackEntry.ModuleType.PASSWORDS);
    }

    private void verifyHatsTrigger(String moduleType) {
        verify(mSafetyHubHatsBridgeNatives, never())
                .triggerHatsSurveyIfEnabled(eq(mProfile), any(WebContents.class), eq(moduleType));

        // Verify that the survey is NOT triggered on an Incognito tab.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL1,
                /* incognito= */ true);
        verify(mSafetyHubHatsBridgeNatives, never())
                .triggerHatsSurveyIfEnabled(eq(mProfile), any(WebContents.class), eq(moduleType));

        // Verify that the survey is triggered on next page load on a regular tab.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL2,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(eq(mProfile), any(WebContents.class), eq(moduleType));

        // Verify that there are no more attempts to trigger the survey.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL1,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(eq(mProfile), any(WebContents.class), eq(moduleType));
    }
}
