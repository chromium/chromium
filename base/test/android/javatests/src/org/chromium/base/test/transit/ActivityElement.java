// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;

import java.util.List;

/**
 * Represents an {@link Activity} that needs to exist to consider the Station active.
 *
 * <p>Subclasses are treated as a different type.
 *
 * @param <ActivityT> exact type of Activity expected
 */
public class ActivityElement<ActivityT extends Activity> extends Element<ActivityT> {
    private final Class<ActivityT> mActivityClass;

    ActivityElement(Class<ActivityT> activityClass) {
        super("AE/" + activityClass.getCanonicalName());
        mActivityClass = activityClass;
    }

    @Override
    public ConditionWithResult<ActivityT> createEnterCondition() {
        return new ActivityExistsCondition();
    }

    @Override
    public @Nullable Condition createExitCondition() {
        return null;
    }

    private class ActivityExistsCondition extends ConditionWithResult<ActivityT> {

        public ActivityExistsCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatusWithResult<ActivityT> resolveWithSuppliers() {
            ActivityT candidate = null;
            List<Activity> allActivities = ApplicationStatus.getRunningActivities();
            for (Activity activity : allActivities) {
                if (mActivityClass.equals(activity.getClass())) {
                    ActivityT matched = mActivityClass.cast(activity);
                    if (candidate != null) {
                        return error("%s matched two Activities: %s, %s", this, candidate, matched)
                                .withoutResult();
                    }
                    candidate = matched;
                }
            }
            if (candidate == null) {
                return awaiting("No Activity with expected class").withoutResult();
            }

            @ActivityState int state = ApplicationStatus.getStateForActivity(candidate);
            String statusString =
                    String.format(
                            "matched: %s (state=%s)", candidate, activityStateDescription(state));
            if (state == ActivityState.RESUMED) {
                return fulfilled(statusString).withResult(candidate);
            } else {
                return awaiting(statusString).withoutResult();
            }
        }

        @Override
        public String buildDescription() {
            return "Activity exists and is RESUMED: " + mActivityClass.getSimpleName();
        }
    }

    private static String activityStateDescription(@ActivityState int state) {
        return switch (state) {
            case ActivityState.CREATED -> "CREATED";
            case ActivityState.STARTED -> "STARTED";
            case ActivityState.RESUMED -> "RESUMED";
            case ActivityState.PAUSED -> "PAUSED";
            case ActivityState.STOPPED -> "STOPPED";
            case ActivityState.DESTROYED -> "DESTROYED";
            default -> throw new IllegalStateException("Unexpected value: " + state);
        };
    }
}
