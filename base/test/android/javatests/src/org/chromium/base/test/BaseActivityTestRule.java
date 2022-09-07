// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.content.Intent;
import android.support.test.runner.lifecycle.Stage;
import android.text.TextUtils;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.ApplicationTestUtils;

/**
 * A replacement for ActivityTestRule, designed for use in Chromium. This implementation supports
 * launching the target activity through a launcher or redirect from another Activity.
 *
 * @param <T> The type of Activity this Rule will use.
 */
public class BaseActivityTestRule<T extends Activity> implements TestRule {
    private static final String TAG = "BaseActivityTestRule";

    private final Class<T> mActivityClass;
    private boolean mFinishActivity = true;
    private T mActivity;

    /**
     * @param activityClass The Class of the Activity the TestRule will use.
     */
    public BaseActivityTestRule(Class<T> activityClass) {
        mActivityClass = activityClass;
    }

    @Override
    @CallSuper
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    if (mFinishActivity && mActivity != null) {
                        ApplicationTestUtils.finishActivity(mActivity);
                    }
                }
            }
        };
    }

    /**
     * @param finishActivity Whether to finish the Activity between tests. This is only meaningful
     *     in the context of {@link Batch} tests. Non-batched tests will always finish Activities
     *     between tests.
     */
    public void setFinishActivity(boolean finishActivity) {
        mFinishActivity = finishActivity;
    }

    /**
     * @return The activity under test.
     */
    public T getActivity() {
        return mActivity;
    }

    /**
     * Set the Activity to be used by this TestRule.
     */
    public void setActivity(T activity) {
        mActivity = activity;
    }

    protected Intent getActivityIntent() {
        return new Intent(ContextUtils.getApplicationContext(), mActivityClass);
    }

    /**
     * Launches the Activity under test using the provided intent. If the provided intent is null,
     * an explicit intent targeting the Activity is created and used.
     */
    public void launchActivity(@Nullable Intent startIntent) {
        if (startIntent == null) {
            startIntent = getActivityIntent();
        } else {
            String packageName = ContextUtils.getApplicationContext().getPackageName();
            Assert.assertTrue(TextUtils.equals(startIntent.getPackage(), packageName)
                    || (startIntent.getComponent() != null
                            && TextUtils.equals(
                                    startIntent.getComponent().getPackageName(), packageName)));
        }

        startIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Log.d(TAG, String.format("Launching activity %s", mActivityClass.getName()));

        final Intent intent = startIntent;
        mActivity = ApplicationTestUtils.waitForActivityWithClass(mActivityClass, Stage.CREATED,
                () -> ContextUtils.getApplicationContext().startActivity(intent));
    }

    /**
     * Recreates the Activity, blocking until finished.
     * After calling this, getActivity() returns the new Activity.
     */
    public void recreateActivity() {
        setActivity(ApplicationTestUtils.recreateActivity(getActivity()));
    }
}
