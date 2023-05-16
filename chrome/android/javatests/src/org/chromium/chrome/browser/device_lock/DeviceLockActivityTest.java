// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.device_lock;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;

import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for the Device Lock Activity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "ActivityScenario tests should run separately, ActivityScenarioRule does "
                + "not support #launchActivityForResult")
public class DeviceLockActivityTest {
    private DeviceLockActivity mDeviceLockActivity;
    private ActivityScenario<DeviceLockActivity> mActivityScenario;

    @After
    public void tearDown() {
        mActivityScenario.close();
        mActivityScenario = null;
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testDeviceLockReady_finishesActivityWithResultOk() {
        launchActivity();
        onView(withText(R.string.device_lock_title)).check(matches(isDisplayed()));

        mDeviceLockActivity.onDeviceLockReady();
        assertEquals("Activity should be finished", Lifecycle.State.DESTROYED,
                mActivityScenario.getState());
        assertEquals("Setting a device lock should set activity result to OK", Activity.RESULT_OK,
                mActivityScenario.getResult().getResultCode());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testDeviceLockRefused_finishesActivityWithResultCanceled() {
        launchActivity();
        onView(withText(R.string.device_lock_title)).check(matches(isDisplayed()));

        mDeviceLockActivity.onDeviceLockRefused();
        assertEquals("Activity should be finished", Lifecycle.State.DESTROYED,
                mActivityScenario.getState());
        assertEquals("Refusing a device lock should set activity result to CANCELED",
                Activity.RESULT_CANCELED, mActivityScenario.getResult().getResultCode());
    }

    public void launchActivity() {
        Intent intent = DeviceLockActivity.createIntent(
                ContextUtils.getApplicationContext(), true, "testSelectedAccount");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivityScenario = ActivityScenario.launchActivityForResult(intent);
        mActivityScenario.onActivity(activity -> mDeviceLockActivity = activity);
    }
}
