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

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionNoticeController.NoticeControllerEvent;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
import java.util.concurrent.ExecutionException;

@RunWith(ChromeJUnit4ClassRunner.class)
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
    public void testRenderNotice() {
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);

        renderViewWithId(R.id.message_banner, "tracking_protection_notice");
    }

    @Test
    @SmallTest
    public void testNoticeShownOnlyOnSecurePage() {
        var notShownWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.CONTROLLER_CREATED,
                                NoticeControllerEvent.ACTIVE_TAB_CHANGED,
                                NoticeControllerEvent.NON_SECURE_CONNECTION,
                                NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN)
                        .build();

        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);

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
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testNoticeNotShownWhenBridgeIsReturningFalse() {
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(false);

        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);
        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testNoticeNotShownMoreThanOnceWhenNewTabWithSecurePageIsOpened() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTICE_CONTROLLER_EVENT_HISTOGRAM,
                                NoticeControllerEvent.CONTROLLER_CREATED,
                                NoticeControllerEvent.ACTIVE_TAB_CHANGED,
                                NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING,
                                NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN)
                        .build();

        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);
        setConnectionSecurityLevel(ConnectionSecurityLevel.SECURE);

        sActivityTestRule.startMainActivityWithURL(UrlConstants.GOOGLE_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));

        sActivityTestRule.loadUrlInNewTab(UrlConstants.MY_ACTIVITY_HOME_URL);
        onView(withId(R.id.message_banner)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testNoticeDismissedWhenPrimaryButtonClicked() {
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);
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
    public void testNoticeDismissedWhenSettingsClicked() {
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);
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
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);
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
    public void testNoticeDismissedByUser() {
        mFakeTrackingProtectionBridge.setShouldShowOnboardingNotice(true);
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

    private void assertLastAction(int action) {
        assertEquals(
                "Last notice action",
                action,
                (int) mFakeTrackingProtectionBridge.getLastOnboardingNoticeAction());
    }

    private void assertNoticeShownActionIsRecorded() {
        assertTrue(mFakeTrackingProtectionBridge.wasOnboardingNoticeShown());
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
