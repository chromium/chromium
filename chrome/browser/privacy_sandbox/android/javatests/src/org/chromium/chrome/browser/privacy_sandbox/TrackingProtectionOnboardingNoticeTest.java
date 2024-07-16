// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;

import java.util.concurrent.ExecutionException;

@RunWith(ChromeJUnit4ClassRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrackingProtectionOnboardingNoticeTest {
    @Rule
    public ChromeTabbedActivityTestRule sActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mocker = new JniMocker();

    private FakeTrackingProtectionBridge mFakeTrackingProtectionBridge;
    @Mock SecurityStateModel.Natives mSecurityStateModelNatives;

    @Before
    public void setUp() throws ExecutionException {
        MockitoAnnotations.openMocks(this);

        mFakeTrackingProtectionBridge = new FakeTrackingProtectionBridge();
        mocker.mock(TrackingProtectionBridgeJni.TEST_HOOKS, mFakeTrackingProtectionBridge);
        mocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateModelNatives);
        setConnectionSecurityLevel(ConnectionSecurityLevel.NONE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_FULL_ONBOARDING_MOBILE_TRIGGER)
    public void testFullTrackingProtectionNoticeDismissedWhenPrimaryButtonClicked() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.FULL3PCD_ONBOARDING);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
        onView(withId(R.id.message_primary_button)).perform(click());
        assertNoticeShownActionIsRecorded();
        assertLastAction(NoticeAction.GOT_IT);

        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_FULL_ONBOARDING_MOBILE_TRIGGER)
    public void testFullTrackingProtectionSilentOnboardingNoticeShown() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.FULL3PCD_SILENT_ONBOARDING);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        // Show the notice.
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);

        assertNoticeShownActionIsRecorded();
    }

    private void assertLastAction(int action) {
        assertEquals(
                "Last notice action",
                action,
                (int) mFakeTrackingProtectionBridge.getLastNoticeAction());
    }

    private void assertNoticeShownActionIsRecorded() {
        assertTrue(mFakeTrackingProtectionBridge.wasNoticeShown());
    }

    private void setConnectionSecurityLevel(int connectionSecurityLevel) {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenAnswer((mock) -> connectionSecurityLevel);
    }
}
