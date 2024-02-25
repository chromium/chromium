// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionNoticeController.NOTICE_CONTROLLER_EVENT_HISTOGRAM;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.espresso.Espresso;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionNoticeController.NoticeControllerEvent;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.concurrent.ExecutionException;

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class TrackingProtectionNoticeTest {
    @Rule
    public ChromeTabbedActivityTestRule sActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.PRIVACY)
                    .build();

    @Rule public JniMocker mocker = new JniMocker();

    private FakeTrackingProtectionBridge mFakeTrackingProtectionBridge;

    @Mock SecurityStateModel.Natives mSecurityStateModelNatives;

    /** Parameter set controlling the notice type. */
    public static class TPNoticeTestParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(NoticeType.ONBOARDING).name("OnboardingNotice"),
                    new ParameterSet().value(NoticeType.OFFBOARDING).name("OffboardingNotice"));
        }
    }

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
    @Feature({"RenderTest"})
    public void testRenderOnboardingNotice() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.ONBOARDING);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);

        renderViewWithId(R.id.message_banner, "tracking_protection_onboarding_notice");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderOffboardingNotice() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.OFFBOARDING);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);

        renderViewWithId(R.id.message_banner, "tracking_protection_offboarding_notice");
    }

    @Test
    @SmallTest
    public void testNoticeShownOnlyOnSecurePage() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING, false);
        FeatureList.setTestValues(testValues);
        var notShownWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.CONTROLLER_CREATED,
                                NoticeControllerEvent.ACTIVE_TAB_CHANGED,
                                NoticeControllerEvent.NON_SECURE_CONNECTION,
                                NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN)
                        .build();

        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.ONBOARDING);

        sActivityTestRule.startMainActivityOnBlankPage();
        onView(withId(R.id.message_banner)).check(doesNotExist());
        notShownWatcher.assertExpected();

        var pageLoadWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.NAVIGATION_FINISHED,
                                NoticeControllerEvent.NON_SECURE_CONNECTION,
                                NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN)
                        .build();
        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.message_banner)).check(doesNotExist());
        pageLoadWatcher.assertExpected();

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.loadUrl(UrlConstants.GOOGLE_URL);
        assertNoticeRequestedActionIsRecorded(false);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testNoticeRequestSentOnSecurePage() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING, true);
        FeatureList.setTestValues(testValues);

        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.ONBOARDING);

        sActivityTestRule.startMainActivityOnBlankPage();
        onView(withId(R.id.message_banner)).check(doesNotExist());

        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.message_banner)).check(doesNotExist());
        assertNoticeRequestedActionIsRecorded(false);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.loadUrl(UrlConstants.GOOGLE_URL);
        assertNoticeRequestedActionIsRecorded(true);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testNoticeNotShownWhenNotRequired() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.NONE);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testNoticeNotShownWhenIncognito() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.ONBOARDING);

        sActivityTestRule.startMainActivityOnBlankPage();

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.loadUrlInNewTab(UrlConstants.GOOGLE_URL, /* incognito= */ true);
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    // TODO(crbug.com/1497465): Fix flakiness on histogramWatcher assertion.
    public void testNoticeNotShownMoreThanOnceWhenNewTabWithSecurePageIsOpened() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.ONBOARDING);
        sActivityTestRule.startMainActivityOnBlankPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.NAVIGATION_FINISHED,
                                NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING,
                                NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN)
                        .build();

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.loadUrl(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
        histogramWatcher.assertExpected();

        sActivityTestRule.loadUrlInNewTab(UrlConstants.MY_ACTIVITY_HOME_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(TPNoticeTestParams.class)
    public void testNoticeDismissedWhenPrimaryButtonClicked(@NoticeType int noticeType) {
        mFakeTrackingProtectionBridge.setRequiredNotice(noticeType);
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
    @ParameterAnnotations.UseMethodParameter(TPNoticeTestParams.class)
    public void testNoticeDismissedWhenSettingsClicked(@NoticeType int noticeType) {
        mFakeTrackingProtectionBridge.setRequiredNotice(noticeType);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        // Show the notice.
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));

        // Click on settings.
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText("Settings")).perform(click());
        assertNoticeShownActionIsRecorded();
        assertLastAction(NoticeAction.SETTINGS);

        // Verify TP Settings page is shown and notice is dismissed.
        onView(withText("Tracking Protections")).check(doesNotExist());
        Espresso.pressBack();
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testNoticeDismissedWhenLearnMoreClicked() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.OFFBOARDING);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        // Show the notice.
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));

        // Click on learn more.
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText("Learn more")).perform(click());
        assertNoticeShownActionIsRecorded();
        assertLastAction(NoticeAction.LEARN_MORE);

        // Verify the notice is dismissed.
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(TPNoticeTestParams.class)
    public void testNoticeDismissedByUser(@NoticeType int noticeType) {
        mFakeTrackingProtectionBridge.setRequiredNotice(noticeType);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        // Show the notice.
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));

        // Dismiss it.
        onView(withId(R.id.message_banner)).perform(swipeUp());

        // Verify the notice is dismissed.
        assertNoticeShownActionIsRecorded();
        assertLastAction(NoticeAction.CLOSED);
        onView(withId(R.id.message_banner)).check(doesNotExist());

        // Verify notice won't be shown again.
        sActivityTestRule.loadUrl(UrlConstants.MY_ACTIVITY_HOME_URL);
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testSilentOnboardingNoticeShown() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.SILENT_ONBOARDING);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        // Show the notice.
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);

        assertNoticeShownActionIsRecorded();
    }

    @Test
    @SmallTest
    public void testSilentOnboardingNoticeShownOnlyOnSecurePage() {
        mFakeTrackingProtectionBridge.setRequiredNotice(NoticeType.SILENT_ONBOARDING);
        sActivityTestRule.startMainActivityOnBlankPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.NAVIGATION_FINISHED,
                                NoticeControllerEvent.NON_SECURE_CONNECTION,
                                NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN)
                        .build();
        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        histogramWatcher.assertExpected();

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.loadUrl(UrlConstants.GOOGLE_URL);
        assertNoticeShownActionIsRecorded();
    }

    private void assertLastAction(int action) {
        assertEquals(
                "Last notice action",
                action,
                (int) mFakeTrackingProtectionBridge.getLastNoticeAction());
    }

    private void assertNoticeRequestedActionIsRecorded(boolean noticeRequested) {
        assertEquals(mFakeTrackingProtectionBridge.wasNoticeRequested(), noticeRequested);
    }

    private void assertNoticeShownActionIsRecorded() {
        assertTrue(mFakeTrackingProtectionBridge.wasNoticeShown());
    }

    private void setConnectionSecurityLevel(int connectionSecurityLevel) {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenAnswer((mock) -> connectionSecurityLevel);
    }

    private void renderViewWithId(int id, String renderId) {
        onViewWaiting(withId(id));
        onView(withId(id))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            // Allow disk writes and slow calls to render from UI thread.
                            try (StrictModeContext ignored =
                                    StrictModeContext.allowAllThreadPolicies()) {
                                TestThreadUtils.runOnUiThreadBlocking(
                                        () -> RenderTestRule.sanitize(v));
                                mRenderTestRule.render(v, renderId);
                            } catch (IOException e) {
                                assert false : "Render test failed due to " + e;
                            }
                        });
    }
}
