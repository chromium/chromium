// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.ApplicationTestUtils.waitForActivityWithClass;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.ActivityResultTrackerImpl;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** Tests for {@link ActivityResultTrackerImpl}. */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ActivityResultTrackerIntegrationTest {
    private static final String KEY = "key";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mBaseActivity;
    private ActivityResultTracker mTracker;
    private Instrumentation mInstrumentation;
    private ActivityMonitor mResultActivityMonitor;

    private static class ActivityResultTester {
        private final CallbackHelper mResultCallbackHelper = new CallbackHelper();
        private final ActivityResultCallback<ActivityResult> mCallback =
                (result) -> {
                    mReceivedResult = result;
                    mResultCallbackHelper.notifyCalled();
                };
        private ActivityResult mReceivedResult;

        ActivityResultTester(ActivityResultTracker tracker, String key) {
            tracker.register(key, mCallback);
        }

        void waitForCallback() throws TimeoutException {
            mResultCallbackHelper.waitForCallback(0);
        }

        void assertCallbackCallCount(int times) {
            Assert.assertEquals(
                    "Callback should have been called " + times + " times.",
                    times,
                    mResultCallbackHelper.getCallCount());
        }

        void assertReceivedResult(int expectedResultCode) {
            assertCallbackCallCount(1);
            Assert.assertNotNull("Activity result should not be null.", mReceivedResult);
            Assert.assertEquals(
                    "Result code is not correct.",
                    expectedResultCode,
                    mReceivedResult.getResultCode());
        }
    }

    @BeforeClass
    public static void setUpSuite() {
        sActivityTestRule.startMainActivityOnBlankPage();
    }

    @Before
    public void setUp() {
        mBaseActivity = sActivityTestRule.getActivity();
        mTracker = mBaseActivity.getActivityResultTracker();
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mResultActivityMonitor =
                mInstrumentation.addMonitor(BlankUiTestActivity.class.getName(), null, false);
    }

    @After
    public void tearDown() {
        mInstrumentation.removeMonitor(mResultActivityMonitor);
        ((ActivityResultTrackerImpl) mTracker).removeAll();
    }

    @Test
    @SmallTest
    public void testStartActivity_withoutRegister() {
        Intent intent = new Intent(mBaseActivity, BlankUiTestActivity.class);
        Assert.assertThrows(IllegalStateException.class, () -> mTracker.startActivity(KEY, intent));
    }

    @Test
    @SmallTest
    public void testStartActivity_withResultOk() throws TimeoutException {
        ActivityResultTester tester = new ActivityResultTester(mTracker, KEY);
        startResultActivity(KEY);
        finishResultActivity(Activity.RESULT_OK);

        tester.waitForCallback();
        tester.assertReceivedResult(Activity.RESULT_OK);
    }

    @Test
    @SmallTest
    public void testStartActivity_withResultCanceled() throws TimeoutException {
        ActivityResultTester tester = new ActivityResultTester(mTracker, KEY);
        startResultActivity(KEY);
        finishResultActivity(Activity.RESULT_CANCELED);

        tester.waitForCallback();
        tester.assertReceivedResult(Activity.RESULT_CANCELED);
    }

    @Test
    @SmallTest
    public void testMultipleActivities() throws TimeoutException {
        // Register two activities with different keys.
        final String key2 = "testKey2";
        ActivityResultTester tester1 = new ActivityResultTester(mTracker, KEY);
        ActivityResultTester tester2 = new ActivityResultTester(mTracker, key2);

        startResultActivity(KEY);
        finishResultActivity(Activity.RESULT_OK);
        tester1.waitForCallback();
        tester1.assertReceivedResult(Activity.RESULT_OK);
        tester2.assertCallbackCallCount(0);

        mInstrumentation.removeMonitor(mResultActivityMonitor);
        mResultActivityMonitor =
                mInstrumentation.addMonitor(BlankUiTestActivity.class.getName(), null, false);
        startResultActivity(key2);
        finishResultActivity(Activity.RESULT_OK);
        tester2.waitForCallback();
        tester2.assertReceivedResult(Activity.RESULT_OK);
        tester1.assertCallbackCallCount(1);
    }

    @Test
    @SmallTest
    public void testRegister_throwIfKeyDuplicated() throws TimeoutException {
        new ActivityResultTester(mTracker, KEY);
        Assert.assertThrows(
                IllegalStateException.class, () -> new ActivityResultTester(mTracker, KEY));
    }

    @Test
    @SmallTest
    public void testStartActivity_activityKilled_withResultOk() throws TimeoutException {
        ActivityResultTester tester = new ActivityResultTester(mTracker, KEY);
        startResultActivity(KEY);
        recreateBaseActivity();
        finishResultActivity(Activity.RESULT_OK);

        // After the activity is recreated, the old `tester`'s callback is unregistered.
        // A new tester needs to be created to listen to the result.
        ActivityResultTester testerAfterRecreation = new ActivityResultTester(mTracker, KEY);

        tester.assertCallbackCallCount(0);
        testerAfterRecreation.waitForCallback();
        testerAfterRecreation.assertReceivedResult(Activity.RESULT_OK);
    }

    @Test
    @SmallTest
    public void testStartActivity_activityKilled_withResultCanceled() throws TimeoutException {
        ActivityResultTester tester = new ActivityResultTester(mTracker, KEY);
        startResultActivity(KEY);
        recreateBaseActivity();
        finishResultActivity(Activity.RESULT_CANCELED);
        // After the activity is recreated, the old `tester`'s callback is unregistered.
        // A new tester needs to be created to listen to the result.
        ActivityResultTester testerAfterRecreation = new ActivityResultTester(mTracker, KEY);

        tester.assertCallbackCallCount(0);
        testerAfterRecreation.waitForCallback();
        testerAfterRecreation.assertReceivedResult(Activity.RESULT_CANCELED);
    }

    @Test
    @SmallTest
    public void testStartActivity_activityKilled_multipleActivities() throws TimeoutException {
        String key2 = "key2";
        ActivityResultTester tester1 = new ActivityResultTester(mTracker, KEY);
        ActivityResultTester tester2 = new ActivityResultTester(mTracker, key2);
        startResultActivity(KEY);
        recreateBaseActivity();
        finishResultActivity(Activity.RESULT_OK);

        // After the activity is recreated, the old `tester`'s callback is unregistered.
        // A new tester needs to be created to listen to the result.
        ActivityResultTester testerAfterRecreation1 = new ActivityResultTester(mTracker, KEY);
        ActivityResultTester testerAfterRecreation2 = new ActivityResultTester(mTracker, key2);

        tester1.assertCallbackCallCount(0);
        tester2.assertCallbackCallCount(0);
        testerAfterRecreation2.assertCallbackCallCount(0);
        testerAfterRecreation1.waitForCallback();
        testerAfterRecreation1.assertReceivedResult(Activity.RESULT_OK);
    }

    private void startResultActivity(String key) {
        Intent intent = new Intent(mBaseActivity, BlankUiTestActivity.class);
        waitForActivityWithClass(
                BlankUiTestActivity.class,
                Stage.RESUMED,
                () -> mTracker.startActivity(key, intent));
    }

    private void finishResultActivity(int resultCode) {
        Activity resultActivity =
                mInstrumentation.waitForMonitorWithTimeout(
                        mResultActivityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Could not find activity.", resultActivity);
        mInstrumentation.runOnMainSync(
                () -> {
                    if (!resultActivity.isFinishing()) {
                        resultActivity.setResult(resultCode, null);
                        resultActivity.finish();
                    }
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return resultActivity.isDestroyed();
                },
                "Activity instance was not destroyed within timeout.",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void recreateBaseActivity() {
        mBaseActivity =
                waitForActivityWithClass(
                        mBaseActivity.getClass(), Stage.CREATED, () -> mBaseActivity.recreate());
        sActivityTestRule.setActivity(mBaseActivity);
        mTracker = mBaseActivity.getActivityResultTracker();
    }
}
