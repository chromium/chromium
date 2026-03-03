// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.device_lock;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;

import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.IntentRequestTracker;

/** Tests for the {@link DeviceLockActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "ActivityScenario tests should run separately, ActivityScenarioRule does "
                        + "not support #launchActivityForResult")
public class DeviceLockActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IntentRequestTracker mIntentRequestTracker;

    private DeviceLockActivity mDeviceLockActivity;
    private ActivityScenario<DeviceLockActivity> mActivityScenario;

    @Before
    public void setUp() {
        Intent intent =
                DeviceLockActivity.createIntent(
                        ContextUtils.getApplicationContext(),
                        new CoreAccountId(new GaiaId("accountId")),
                        true,
                        DeviceLockActivityLauncher.Source.ACCOUNT_PICKER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivityScenario = ActivityScenario.launchActivityForResult(intent);
        mActivityScenario.onActivity(activity -> mDeviceLockActivity = activity);
        ApplicationTestUtils.waitForActivityState(mDeviceLockActivity, Stage.RESUMED);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
    }

    @Test
    @MediumTest
    public void testDeviceLockReady_finishesActivityWithResultOk() {
        onView(withText(R.string.device_lock_description)).check(matches(isDisplayed()));

        mDeviceLockActivity.onDeviceLockReady();
        assertTrue("Activity should be finished", mDeviceLockActivity.isFinishing());
        assertEquals(
                "Setting a device lock should set activity result to OK",
                Activity.RESULT_OK,
                mActivityScenario.getResult().getResultCode());
        ApplicationTestUtils.waitForActivityState(mDeviceLockActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testDeviceLockRefused_finishesActivityWithResultCanceled() {
        onView(withText(R.string.device_lock_description)).check(matches(isDisplayed()));

        mDeviceLockActivity.onDeviceLockRefused();

        assertTrue("Activity should be finished", mDeviceLockActivity.isFinishing());
        assertEquals(
                "Refusing a device lock should set activity result to CANCELED",
                Activity.RESULT_CANCELED,
                mActivityScenario.getResult().getResultCode());
        ApplicationTestUtils.waitForActivityState(mDeviceLockActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testOnActivityResult_passesActivityResultToWindowAndroid() {
        mDeviceLockActivity.setIntentRequestTrackerForTesting(mIntentRequestTracker);

        int testRequestCode = 1;
        Intent data = new Intent();
        mDeviceLockActivity.onActivityResult(testRequestCode, Activity.RESULT_OK, data);

        verify(mIntentRequestTracker).onActivityResult(testRequestCode, Activity.RESULT_OK, data);
    }
}
