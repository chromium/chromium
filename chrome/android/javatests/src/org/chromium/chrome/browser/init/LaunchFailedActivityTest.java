// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Integration test suite for the failure to launch dialog..
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test interacts with startup.")
public class LaunchFailedActivityTest {
    private static final String TEST_URL = "https://test.com";
    private static final long ACTIVITY_WAIT_LONG_MS = TimeUnit.SECONDS.toMillis(10);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private final Set<Class> mSupportedActivities = CollectionUtil.newHashSet(
            ChromeLauncherActivity.class, LaunchFailedActivity.class, ChromeTabbedActivity.class);
    private final Map<Class, ActivityMonitor> mMonitorMap = new HashMap<>();
    private Instrumentation mInstrumentation;
    private Context mContext;
    private Activity mLastActivity;

    @Before
    public void setUp() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        for (Class clazz : mSupportedActivities) {
            ActivityMonitor monitor = new ActivityMonitor(clazz.getName(), null, false);
            mMonitorMap.put(clazz, monitor);
            mInstrumentation.addMonitor(monitor);
        }
    }

    @After
    public void tearDown() {
        // Tear down the last activity first, otherwise the other cleanup, in particular skipped by
        // policy pref, might trigger an assert in activity initialization because of the statics
        // we reset below. Run it on UI so there are no threading issues.
        if (mLastActivity != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> mLastActivity.finish());
        }
        // Finish the rest of the running activities.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                runningActivity.finish();
            }
        });
    }

    private <T extends Activity> T waitForActivity(Class<T> activityClass) {
        Assert.assertTrue(Arrays.toString(mSupportedActivities.toArray()) + " doesn't contain "
                        + activityClass,
                mSupportedActivities.contains(activityClass));
        ActivityMonitor monitor = mMonitorMap.get(activityClass);
        mLastActivity = mInstrumentation.waitForMonitorWithTimeout(monitor, ACTIVITY_WAIT_LONG_MS);
        Assert.assertNotNull("Could not find " + activityClass.getName(), mLastActivity);
        return (T) mLastActivity;
    }

    @Test
    @SmallTest
    public void testHandlesLaunchFailed() {
        AsyncInitializationActivity.sOverrideNativeLibraryCannotBeLoadedForTesting = true;
        ResettersForTesting.register(() -> {
            AsyncInitializationActivity.sOverrideNativeLibraryCannotBeLoadedForTesting = null;
        });

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        // Because the device is incompatible,  AsyncInitializationActivity notices that the FRE
        // hasn't been run yet, it redirects to it.  Once the user closes the FRE, the user should
        // be kicked back into the startup flow where they were interrupted.
        waitForActivity(LaunchFailedActivity.class);

        Assert.assertFalse(mLastActivity.isFinishing());
    }
}
