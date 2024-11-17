// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.createSecondChromeTabbedActivity;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Class for testing MultiWindowUtils. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
public class MultiWindowUtilsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Mock private MultiWindowUtils mMultiWindowUtils;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mMultiWindowUtils = Mockito.spy(MultiWindowUtils.getInstance());
    }

    @After
    public void teardown() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
    }

    /** Tests that ChromeTabbedActivity2 is used for intents when EXTRA_WINDOW_ID is set to 2. */
    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    @Feature("MultiWindow")
    public void testTabbedActivityForIntentWithExtraWindowId() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        createSecondChromeTabbedActivity(activity1);

        Intent intent = activity1.getIntent();
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, 2);

        Assert.assertEquals(
                "ChromeTabbedActivity2 should be used when EXTRA_WINDOW_ID is set to 2.",
                ChromeTabbedActivity2.class,
                MultiWindowUtils.getInstance().getTabbedActivityForIntent(intent, activity1));
    }

    /**
     * Tests that if two ChromeTabbedActivities are running the one that was resumed most recently
     * is used as the class name for new intents.
     */
    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    @Feature("MultiWindow")
    public void testTabbedActivityForIntentLastResumedActivity() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        final ChromeTabbedActivity2 activity2 = createSecondChromeTabbedActivity(activity1);

        Assert.assertFalse(
                "ChromeTabbedActivity should not be resumed",
                ApplicationStatus.getStateForActivity(activity1) == ActivityState.RESUMED);
        Assert.assertTrue(
                "ChromeTabbedActivity2 should be resumed",
                ApplicationStatus.getStateForActivity(activity2) == ActivityState.RESUMED);

        // Wait for profile to be initialized.
        CriteriaHelper.pollUiThread(() -> activity2.getCurrentTabModel().getProfile() != null);

        // Open settings and wait for ChromeTabbedActivity2 to pause.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity2.onMenuOrKeyboardAction(R.id.preferences_id, true);
                });
        int expected = ActivityState.PAUSED;
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getStateForActivity(activity2),
                            Matchers.is(expected));
                });

        Assert.assertEquals(
                "The most recently resumed ChromeTabbedActivity should be used for intents.",
                ChromeTabbedActivity2.class,
                MultiWindowUtils.getInstance()
                        .getTabbedActivityForIntent(activity1.getIntent(), activity1));
    }

    /**
     * Tests that if only ChromeTabbedActivity is running it is used as the class name for intents.
     */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    @DisabledTest(message = "https://crbug.com/1417018")
    public void testTabbedActivityForIntentOnlyActivity1IsRunning() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        ChromeTabbedActivity2 activity2 = createSecondChromeTabbedActivity(activity1);
        activity2.finishAndRemoveTask();

        Assert.assertEquals(
                "ChromeTabbedActivity should be used for intents if ChromeTabbedActivity2 is "
                        + "not running.",
                ChromeTabbedActivity.class,
                MultiWindowUtils.getInstance()
                        .getTabbedActivityForIntent(activity1.getIntent(), activity1));
    }

    /**
     * Tests that if only ChromeTabbedActivity2 is running it is used as the class name for intents.
     */
    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    @Feature("MultiWindow")
    public void testTabbedActivityForIntentOnlyActivity2IsRunning() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        createSecondChromeTabbedActivity(activity1);
        activity1.finishAndRemoveTask();

        Assert.assertEquals(
                "ChromeTabbedActivity2 should be used for intents if ChromeTabbedActivity is "
                        + "not running.",
                ChromeTabbedActivity2.class,
                MultiWindowUtils.getInstance()
                        .getTabbedActivityForIntent(activity1.getIntent(), activity1));
    }

    /**
     * Tests that if no ChromeTabbedActivities are running ChromeTabbedActivity is used as the
     * default for intents.
     */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testTabbedActivityForIntentNoActivitiesAlive() {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        activity1.finishAndRemoveTask();

        Assert.assertEquals(
                "ChromeTabbedActivity should be used as the default for external intents.",
                ChromeTabbedActivity.class,
                MultiWindowUtils.getInstance()
                        .getTabbedActivityForIntent(activity1.getIntent(), activity1));
    }

    /** Tests that MultiWindowUtils properly tracks whether ChromeTabbedActivity2 is running. */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    @DisabledTest(message = "https://crbug.com/1417018")
    public void testTabbedActivity2TaskRunning() {
        ChromeTabbedActivity activity2 =
                createSecondChromeTabbedActivity(mActivityTestRule.getActivity());
        Assert.assertTrue(MultiWindowUtils.getInstance().getTabbedActivity2TaskRunning());

        activity2.finishAndRemoveTask();
        MultiWindowUtils.getInstance()
                .getTabbedActivityForIntent(
                        mActivityTestRule.getActivity().getIntent(),
                        mActivityTestRule.getActivity());
        Assert.assertFalse(MultiWindowUtils.getInstance().getTabbedActivity2TaskRunning());
    }

    /**
     * Tests that {@link MultiWindowUtils#areMultipleChromeInstancesRunning} behaves correctly in
     * the case the second instance is killed first.
     */
    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    @Feature("MultiWindow")
    public void testAreMultipleChromeInstancesRunningSecondInstanceKilledFirst()
            throws TimeoutException {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        Assert.assertFalse(
                "Only a single instance should be running at the start.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));

        CallbackHelper activity1StoppedCallback = new CallbackHelper();
        CallbackHelper activity1DestroyedCallback = new CallbackHelper();
        CallbackHelper activity1ResumedCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activity1StateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.STOPPED:
                                activity1StoppedCallback.notifyCalled();
                                break;
                            case ActivityState.DESTROYED:
                                activity1DestroyedCallback.notifyCalled();
                                break;
                            case ActivityState.RESUMED:
                                activity1ResumedCallback.notifyCalled();
                                break;
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            activity1StateListener, activity1);
                });

        // Starting activity2 will stop activity1 as this is not truly multi-window mode.
        int activity1CallCount = activity1StoppedCallback.getCallCount();
        ChromeTabbedActivity activity2 = createSecondChromeTabbedActivity(activity1);
        activity1StoppedCallback.waitForCallback(activity1CallCount);
        Assert.assertTrue(
                "Both instances should be running now that the second has started.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));

        CallbackHelper activity2DestroyedCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activity2StateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.DESTROYED:
                                activity2DestroyedCallback.notifyCalled();
                                break;
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            activity2StateListener, activity2);
                });

        // activity1 may have been destroyed in the background. After destroying activity2 it is
        // necessary to make sure activity1 gets resumed.
        activity1CallCount = activity1ResumedCallback.getCallCount();
        activity2.finishAndRemoveTask();
        activity2DestroyedCallback.waitForOnly();
        activity1ResumedCallback.waitForCallback(activity1CallCount);
        Assert.assertFalse(
                "Only a single instance should be running after the second is killed.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));

        // activity1 may have been destroyed in the background and now it is in the foreground.
        // Wait on the next destroyed call rather than the first.
        activity1CallCount = activity1DestroyedCallback.getCallCount();
        activity1.finishAndRemoveTask();
        activity1DestroyedCallback.waitForCallback(activity1CallCount);
        Assert.assertFalse(
                "No instances should be running as all instances are killed.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));
    }

    /**
     * Tests that {@link MultiWindowUtils#areMultipleChromeInstancesRunning} behaves correctly in
     * the case the first instance is killed first.
     *
     * <p>TODO(crbug.com/40129069): This testcase is restricted to O+ as on Android N calling {@link
     * Activity#finishAndRemoveTask()} on the backgrounded activity1 will not cause it to be
     * DESTROYED it until after activity2 is PAUSED. On O+ activity1 will be DESTROYED immediately.
     * This test should be changed such that it works on N.
     */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    @DisableIf.Build(
            sdk_is_less_than = Build.VERSION_CODES.O,
            message = "https://crbug.com/1077249")
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338976206
    public void testAreMultipleChromeInstancesRunningFirstInstanceKilledFirst()
            throws TimeoutException {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        Assert.assertFalse(
                "Only a single instance should be running at the start.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));

        CallbackHelper activity1StoppedCallback = new CallbackHelper();
        CallbackHelper activity1DestroyedCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activity1StateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.STOPPED:
                                activity1StoppedCallback.notifyCalled();
                                break;
                            case ActivityState.DESTROYED:
                                activity1DestroyedCallback.notifyCalled();
                                break;
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            activity1StateListener, activity1);
                });

        // Starting activity2 will stop activity1 as this is not truly multi-window mode.
        // activity1 may be killed in the background, but since it is never foregrounded again
        // there should be only one call for both stopped and destroyed in this test.
        ChromeTabbedActivity activity2 = createSecondChromeTabbedActivity(activity1);
        activity1StoppedCallback.waitForOnly();
        Assert.assertTrue(
                "Both instances should be running now that the second has started.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity1));

        CallbackHelper activity2DestroyedCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activity2StateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.DESTROYED:
                                activity2DestroyedCallback.notifyCalled();
                                break;
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            activity2StateListener, activity2);
                });

        activity1.finishAndRemoveTask();
        activity1DestroyedCallback.waitForOnly();
        Assert.assertFalse(
                "Only a single instance should be running after the first is killed.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity2));

        // activity2 is always in the foreground so this should be the first time it is destroyed.
        activity2.finishAndRemoveTask();
        activity2DestroyedCallback.waitForOnly();
        Assert.assertFalse(
                "No instances should be running as all instances are killed.",
                MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(activity2));
    }

    /**
     * These tests check that MultiWindowUtils properly checks whether opening tabs in other windows
     * is supported.
     */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testIsOpenInOtherWindowSupported_isNotInMultiWindowDisplayMode_returnsFalse() {
        assertFalse(
                doTestIsOpenInOtherWindowSupported(
                        /* isAutomotive= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* openInOtherWindowActivity= */ ChromeTabbedActivity.class));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testIsOpenInOtherWindowSupported_isAutomotive_returnsFalse() {
        assertFalse(
                doTestIsOpenInOtherWindowSupported(
                        /* isAutomotive= */ true,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ true,
                        /* openInOtherWindowActivity= */ ChromeTabbedActivity.class));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testIsOpenInOtherWindowSupported_otherWindowActivityIsNull_returnsFalse() {
        assertFalse(
                doTestIsOpenInOtherWindowSupported(
                        /* isAutomotive= */ false,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ true,
                        /* openInOtherWindowActivity= */ null));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testIsOpenInOtherWindowSupported_otherWindowActivityIsNotNull_returnsTrue() {
        assertTrue(
                doTestIsOpenInOtherWindowSupported(
                        /* isAutomotive= */ false,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ true,
                        /* openInOtherWindowActivity= */ ChromeTabbedActivity.class));
    }

    public boolean doTestIsOpenInOtherWindowSupported(
            boolean isAutomotive,
            boolean isInMultiWindowMode,
            boolean isInMultiDisplayMode,
            Class<? extends Activity> openInOtherWindowActivity) {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(isAutomotive);

        doReturn(isInMultiWindowMode).when(mMultiWindowUtils).isInMultiWindowMode(any());
        doReturn(isInMultiDisplayMode).when(mMultiWindowUtils).isInMultiDisplayMode(any());
        doReturn(openInOtherWindowActivity)
                .when(mMultiWindowUtils)
                .getOpenInOtherWindowActivity(any());

        return mMultiWindowUtils.isOpenInOtherWindowSupported(mActivityTestRule.getActivity());
    }

    /**
     * These tests check that MultiWindowUtils properly checks whether Chrome can enter multi-window
     * mode.
     */
    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testCanEnterMultiWindowMode_isAutomotive_returnsFalse() {
        assertFalse(
                doTestCanEnterMultiWindowMode(
                        /* isAutomotive= */ true,
                        /* aospMultiWindowModeSupported= */ false,
                        /* customMultiWindowModeSupported= */ false));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testCanEnterMultiWindowMode_noSupport_returnsFalse() {
        assertFalse(
                doTestCanEnterMultiWindowMode(
                        /* isAutomotive= */ false,
                        /* aospMultiWindowModeSupported= */ false,
                        /* customMultiWindowModeSupported= */ false));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testCanEnterMultiWindowMode_aospMultiWindowModeSupported_returnsFalse() {
        assertTrue(
                doTestCanEnterMultiWindowMode(
                        /* isAutomotive= */ false,
                        /* aospMultiWindowModeSupported= */ true,
                        /* customMultiWindowModeSupported= */ false));
    }

    @Test
    @SmallTest
    @Feature("MultiWindow")
    public void testCanEnterMultiWindowMode_customMultiWindowModeSupported_returnsFalse() {
        assertTrue(
                doTestCanEnterMultiWindowMode(
                        /* isAutomotive= */ false,
                        /* aospMultiWindowModeSupported= */ false,
                        /* customMultiWindowModeSupported= */ true));
    }

    public boolean doTestCanEnterMultiWindowMode(
            boolean isAutomotive,
            boolean aospMultiWindowModeSupported,
            boolean customMultiWindowModeSupported) {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(isAutomotive);

        doReturn(aospMultiWindowModeSupported)
                .when(mMultiWindowUtils)
                .aospMultiWindowModeSupported();
        doReturn(customMultiWindowModeSupported)
                .when(mMultiWindowUtils)
                .customMultiWindowModeSupported();

        return mMultiWindowUtils.canEnterMultiWindowMode(mActivityTestRule.getActivity());
    }
}
