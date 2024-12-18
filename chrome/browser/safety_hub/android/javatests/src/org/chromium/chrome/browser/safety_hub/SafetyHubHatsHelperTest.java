// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.ExecutionException;

/** Tests for {@link SafetyHubHatsHelper}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "fullyLoadUrlInNewTab is unhappy when batched.")
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB_ANDROID_SURVEY,
    ChromeFeatureList.SAFETY_HUB_ANDROID_SURVEY_V2
})
public class SafetyHubHatsHelperTest {
    private static final String TEST_URL1 = "https://www.example.com/";
    private static final String TEST_URL2 = "https://www.google.com/";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private SafetyHubHatsBridge.Natives mSafetyHubHatsBridgeNatives;
    @Mock private SafetyHubFetchService mSafetyHubFetchService;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingBridgeNativeMock;

    private FakeUnusedSitePermissionsBridge mUnusedPermissionsBridge =
            new FakeUnusedSitePermissionsBridge();

    private FakeNotificationPermissionReviewBridge mNotificationPermissionReviewBridge =
            new FakeNotificationPermissionReviewBridge();

    private TabModelSelector mTabModelSelector;
    private Profile mProfile;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws ExecutionException {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);

        mJniMocker.mock(SafetyHubHatsBridgeJni.TEST_HOOKS, mSafetyHubHatsBridgeNatives);
        doReturn(true)
                .when(mSafetyHubHatsBridgeNatives)
                .triggerHatsSurveyIfEnabled(any(), any(), any(), anyBoolean(), anyBoolean(), any());

        SafetyHubFetchServiceFactory.setSafetyHubFetchServiceForTesting(mSafetyHubFetchService);
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        doReturn(updateStatus).when(mSafetyHubFetchService).getUpdateStatus();

        mJniMocker.mock(UnusedSitePermissionsBridgeJni.TEST_HOOKS, mUnusedPermissionsBridge);
        mJniMocker.mock(
                NotificationPermissionReviewBridgeJni.TEST_HOOKS,
                mNotificationPermissionReviewBridge);

        mUnusedPermissionsBridge.setPermissionsDataForReview(new PermissionsData[] {});
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {});

        mJniMocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSafeBrowsingBridgeNativeMock);
        doReturn(SafeBrowsingState.NO_SAFE_BROWSING)
                .when(mSafeBrowsingBridgeNativeMock)
                .getSafeBrowsingState(mProfile);

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mProfile = ProfileManager.getLastUsedRegularProfile());
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @SmallTest
    public void testTriggerControlHatsSurvey() {
        SafetyHubHatsHelper safetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);
        safetyHubHatsHelper.triggerControlHatsSurvey(mTabModelSelector);

        verifyHatsTrigger(SafetyHubHatsHelper.CONTROL_NOTIFICATION_MODULE, false);
    }

    @Test
    @SmallTest
    public void testTriggerProactiveHatsSurvey_WhenCardShown() {
        SafetyHubHatsHelper safetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardShown(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);

        verifyHatsTrigger(MagicStackEntry.ModuleType.PASSWORDS, false);
    }

    @Test
    @SmallTest
    public void testTriggerProactiveHatsSurvey_WhenCardTapped() {
        SafetyHubHatsHelper safetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);

        verifyHatsTrigger(MagicStackEntry.ModuleType.PASSWORDS, true);
    }

    @Test
    @SmallTest
    public void testTriggerProactiveHatsSurvey_WhenCardTappedAndShown() {
        SafetyHubHatsHelper safetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);

        // Verify that the survey is triggered on next page load on a regular tab.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL1,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(MagicStackEntry.ModuleType.PASSWORDS),
                        eq(true),
                        eq(false),
                        any());

        // If another survey is attempted to be shown after the a tap has occurred, we should not
        // return that a card was tapped.
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardShown(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL2,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(MagicStackEntry.ModuleType.PASSWORDS),
                        eq(false),
                        eq(false),
                        any());
    }

    @Test
    @SmallTest
    public void testTriggerProactiveHatsSurvey_WhenCardTappedAndShown_WithTwoConsecutiveTriggers() {
        SafetyHubHatsHelper safetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);
        // If another survey is attempted to be shown in a row, the latest data should be attached.
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardShown(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);
        safetyHubHatsHelper.triggerProactiveHatsSurveyWhenCardTapped(
                mTabModelSelector, MagicStackEntry.ModuleType.PASSWORDS);
        // Only the last survey values should be used.
        verifyHatsTrigger(MagicStackEntry.ModuleType.PASSWORDS, true);
    }

    private void verifyHatsTrigger(String moduleType, boolean hasTappedCard) {
        verify(mSafetyHubHatsBridgeNatives, never())
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(moduleType),
                        eq(hasTappedCard),
                        eq(false),
                        any());

        // Verify that the survey is NOT triggered on an Incognito tab.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL1,
                /* incognito= */ true);
        verify(mSafetyHubHatsBridgeNatives, never())
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(moduleType),
                        eq(hasTappedCard),
                        eq(false),
                        any());

        // Verify that the survey is triggered on next page load on a regular tab.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL2,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(moduleType),
                        eq(hasTappedCard),
                        eq(false),
                        any());

        // Verify that there are no more attempts to trigger the survey.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                TEST_URL1,
                /* incognito= */ false);
        verify(mSafetyHubHatsBridgeNatives, times(1))
                .triggerHatsSurveyIfEnabled(
                        eq(mProfile),
                        any(WebContents.class),
                        eq(moduleType),
                        eq(hasTappedCard),
                        eq(false),
                        any());
    }
}
