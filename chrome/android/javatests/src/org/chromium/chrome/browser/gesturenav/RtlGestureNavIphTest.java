// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.os.Build;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.LocalizationUtils;

/** Integration test to test if the IPH dialog can be shown when conditions are met. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.BACK_FORWARD_TRANSITIONS})
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.Q,
        message = " only works in 3-button mode")
@DisabledTest(message = "Test is flaky. See https://crbug.com/357884951")
@Batch(Batch.PER_CLASS)
public class RtlGestureNavIphTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/navigate/simple.html";

    private RtlGestureNavIphController mRtlGestureNavIphController;

    private static class TestTracker implements Tracker {
        private @Nullable String mEmittedEvent;

        @Override
        public void notifyEvent(String event) {
            mEmittedEvent = event;
        }

        public @Nullable String getLastEmittedEvent() {
            return mEmittedEvent;
        }

        @Override
        public boolean shouldTriggerHelpUI(String feature) {
            return true;
        }

        @Override
        public TriggerDetails shouldTriggerHelpUIWithSnooze(String feature) {
            return null;
        }

        @Override
        public boolean wouldTriggerHelpUI(String feature) {
            return true;
        }

        @Override
        public boolean hasEverTriggered(String feature, boolean fromWindow) {
            return true;
        }

        @Override
        public int getTriggerState(String feature) {
            return mEmittedEvent != null
                    ? TriggerState.HAS_BEEN_DISPLAYED
                    : TriggerState.HAS_NOT_BEEN_DISPLAYED;
        }

        @Override
        public void dismissed(String feature) {}

        @Override
        public void dismissedWithSnooze(String feature, int snoozeAction) {}

        @Nullable
        @Override
        public DisplayLockHandle acquireDisplayLock() {
            return () -> {};
        }

        @Override
        public void setPriorityNotification(String feature) {}

        @Override
        public @Nullable String getPendingPriorityNotification() {
            return null;
        }

        @Override
        public void registerPriorityNotificationHandler(
                String feature, Runnable priorityNotificationHandler) {}

        @Override
        public void unregisterPriorityNotificationHandler(String feature) {}

        @Override
        public boolean isInitialized() {
            return true;
        }

        @Override
        public void addOnInitializedCallback(Callback<Boolean> callback) {}
    }

    @Before
    public void setUp() throws InterruptedException {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        LocalizationUtils.setRtlForTesting(true);
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        CompositorAnimationHandler.setTestingMode(true);
        TrackerFactory.setTrackerForTests(new TestTracker());
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=IPH_RtlGestureNavigation<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:availability/any/"
                + "event_trigger/"
                + "name%3Artl_gesture_iph_trigger;comparator%3A==0;window%3A30;storage%3A365/"
                + "event_used/"
                + "name%3Artl_gesture_iph_show;comparator%3A==0;window%3A365;storage%3A365/"
                + "session_rate/<1"
    })
    public void testShowIphOnFailedSwipe() throws InterruptedException {
        mRtlGestureNavIphController =
                ((TabbedRootUiCoordinator)
                                mActivityTestRule.getActivity().getRootUiCoordinatorForTesting())
                        .getRtlGestureNavIphControllerForTesting();
        Assert.assertFalse(mRtlGestureNavIphController.shouldShowOnNonEmptyStack());
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE_2));

        WebContents webContents = mActivityTestRule.getWebContents();
        float widthPx =
                webContents.getWidth() * Coordinates.createFor(webContents).getDeviceScaleFactor();
        float heightPx =
                webContents.getHeight() * Coordinates.createFor(webContents).getDeviceScaleFactor();
        // Two failed swipe gestures in a row.
        for (int i = 0; i < 2; i++) {
            TouchCommon.performDrag(
                    mActivityTestRule.getActivity(),
                    widthPx - 5f,
                    widthPx * 0.6f,
                    /* fromY= */ heightPx * 0.5f,
                    /* toY= */ heightPx * 0.5f,
                    /* stepCount= */ 100,
                    /* duration= */ 600);
            UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        }

        Tracker tracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TrackerFactory.getTrackerForProfile(
                                        mActivityTestRule.getProfile(false)));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        mActivityTestRule
                                        .getActivity()
                                        .getModalDialogManagerSupplier()
                                        .get()
                                        .getCurrentDialogForTest()
                                != null,
                "Dialog should be showing");
        ThreadUtils.runOnUiThread(
                () -> {
                    Assert.assertEquals(
                            "IPH dialog should have been shown",
                            TriggerState.HAS_BEEN_DISPLAYED,
                            tracker.getTriggerState(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION));
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=IPH_RtlGestureNavigation<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:availability/any/"
                + "event_trigger/"
                + "name%3Artl_gesture_iph_trigger;comparator%3A==0;window%3A30;storage%3A365/"
                + "event_used/"
                + "name%3Artl_gesture_iph_show;comparator%3A==0;window%3A365;storage%3A365/"
                + "session_rate/<1/"
                + "x_trigger/non-empty-stack"
    })
    public void testShowIphOnNonEmptyHistoryStack() throws InterruptedException {
        mRtlGestureNavIphController =
                ((TabbedRootUiCoordinator)
                                mActivityTestRule.getActivity().getRootUiCoordinatorForTesting())
                        .getRtlGestureNavIphControllerForTesting();
        Assert.assertTrue(mRtlGestureNavIphController.shouldShowOnNonEmptyStack());
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE_2));
        Tracker tracker =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TrackerFactory.getTrackerForProfile(
                                        mActivityTestRule.getProfile(false)));
        Assert.assertNotNull(
                "Dialog should be showing",
                mActivityTestRule
                        .getActivity()
                        .getModalDialogManagerSupplier()
                        .get()
                        .getCurrentDialogForTest());
        ThreadUtils.runOnUiThread(
                () -> {
                    Assert.assertEquals(
                            "IPH dialog should have been shown",
                            TriggerState.HAS_BEEN_DISPLAYED,
                            tracker.getTriggerState(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION));
                });
    }
}
