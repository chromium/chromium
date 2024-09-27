// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import static com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResultUtils.matchesCheckNames;

import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.is;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.test.espresso.contrib.AccessibilityChecks;
import androidx.test.runner.lifecycle.Stage;

import com.google.android.apps.common.testing.accessibility.framework.checks.ClickableSpanCheck;
import com.google.android.apps.common.testing.accessibility.framework.checks.DuplicateClickableBoundsCheck;
import com.google.android.apps.common.testing.accessibility.framework.checks.EditableContentDescCheck;
import com.google.android.apps.common.testing.accessibility.framework.checks.SpeakableTextPresentCheck;
import com.google.android.apps.common.testing.accessibility.framework.checks.TouchTargetSizeCheck;

import org.junit.Assert;
import org.junit.rules.ExternalResource;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.ApplicationTestUtils;

/**
 * A replacement for ActivityTestRule, designed for use in Chromium. This implementation supports
 * launching the target activity through a launcher or redirect from another Activity.
 *
 * @param <T> The type of Activity this Rule will use.
 */
public class BaseActivityTestRule<T extends Activity> extends ExternalResource {
    private static final String TAG = "BaseActivityTestRule";

    private final Class<T> mActivityClass;
    private boolean mFinishActivity = true;
    private T mActivity;

    /**
     * @param activityClass The Class of the Activity the TestRule will use.
     */
    public BaseActivityTestRule(Class<T> activityClass) {
        mActivityClass = activityClass;

        // Enable accessibility checks, but suppress checks that fit into the following:
        //
        //   TouchTargetSize checks - Many views in Chrome give false positives for the minimum
        //                            target size of 48dp. 100s of tests fail, leave disabled
        //                            until a complete audit can be done.
        //
        //   ClickableSpan checks - Chrome uses ClickableSpan's throughout for in-line links,
        //                          but a URLSpan is considered more accessible, except in the
        //                          case of relative links. Disable until after an audit.
        //
        //   EditableContentDesc checks - Editable TextViews (EditText's) should not have a
        //                                content description and instead have a hint or label.
        //                                Various Autofill tests fail because of this, leave
        //                                disabled until after an audit.
        //
        //   DuplicateClickableBounds checks - Some containers are marked clickable when they do not
        //                                     process click events. Two views with the same bounds
        //                                     should not both be clickable. Some examples in:
        //                                     PageInfoRowView and TabModal.
        //
        //   SpeakableTextPresent* checks - Some views are failing this test on certain try bots,
        //                                  so disable this check to reduce churn for sheriffs
        //                                  until issue can be found. Some examples in:
        //                                  AccessibilitySettings, ReaderMode, and Feedv2 tests.
        //
        // TODO(AccessibilityChecks): Complete above audits and ideally suppress no checks.
        try {
            AccessibilityChecks.enable()
                    .setSuppressingResultMatcher(
                            anyOf(
                                    matchesCheckNames(
                                            is(TouchTargetSizeCheck.class.getSimpleName())),
                                    matchesCheckNames(is(ClickableSpanCheck.class.getSimpleName())),
                                    matchesCheckNames(
                                            is(EditableContentDescCheck.class.getSimpleName())),
                                    matchesCheckNames(
                                            is(
                                                    DuplicateClickableBoundsCheck.class
                                                            .getSimpleName())),
                                    matchesCheckNames(
                                            is(SpeakableTextPresentCheck.class.getSimpleName()))));
        } catch (IllegalStateException e) {
            // Suppress IllegalStateException for AccessibilityChecks already enabled.
        }
    }

    @Override
    @CallSuper
    protected void after() {
        if (mFinishActivity && mActivity != null) {
            finishActivity();
        }
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

    /** Set the Activity to be used by this TestRule. */
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
            Assert.assertTrue(
                    TextUtils.equals(startIntent.getPackage(), packageName)
                            || (startIntent.getComponent() != null
                                    && TextUtils.equals(
                                            startIntent.getComponent().getPackageName(),
                                            packageName)));
        }

        startIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Log.d(TAG, String.format("Launching activity %s", mActivityClass.getName()));

        final Intent intent = startIntent;
        // Android system pauses the activity on delivering an intent to an existing activity.
        // https://developer.android.com/reference/android/app/Activity#onNewIntent(android.content.Intent)
        Stage targetStage =
                ((startIntent.getFlags() & Intent.FLAG_ACTIVITY_SINGLE_TOP) != 0
                                && mActivity != null
                                && !mActivity.isFinishing())
                        ? Stage.PAUSED
                        : Stage.CREATED;
        mActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        mActivityClass,
                        targetStage,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
    }

    /**
     * Finishes the Activity, blocking until finished. After calling this, getActivity() returns
     * null.
     */
    public void finishActivity() {
        ApplicationTestUtils.finishActivity(getActivity());
        setActivity(null);
    }

    /**
     * Recreates the Activity, blocking until finished. After calling this, getActivity() returns
     * the new Activity.
     */
    public void recreateActivity() {
        setActivity(ApplicationTestUtils.recreateActivity(getActivity()));
    }
}
