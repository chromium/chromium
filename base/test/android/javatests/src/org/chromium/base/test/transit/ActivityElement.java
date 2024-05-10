// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.Supplier;

import java.util.List;
import java.util.Set;

/**
 * Represents an {@link Activity} that needs to exist to consider the Station active.
 *
 * <p>Subclasses are treated as a different type.
 *
 * @param <ActivityT> exact type of Activity expected
 */
public class ActivityElement<ActivityT extends Activity>
        implements ElementInState, Supplier<ActivityT> {
    private final Class<ActivityT> mActivityClass;
    private final String mId;
    private final ActivityExistsCondition mEnterCondition;

    ActivityElement(Class<ActivityT> activityClass) {
        mActivityClass = activityClass;
        mId = "AE/" + activityClass.getCanonicalName();
        mEnterCondition = new ActivityExistsCondition();
    }

    @Override
    public String getId() {
        return mId;
    }

    @Override
    public Condition getEnterCondition() {
        return mEnterCondition;
    }

    @Override
    public @Nullable Condition getExitCondition(Set<String> destinationElementIds) {
        return null;
    }

    @Override
    public String toString() {
        return mId;
    }

    @Override
    public ActivityT get() {
        return mEnterCondition.mMatchedActivity;
    }

    @Override
    public boolean hasValue() {
        return mEnterCondition.mMatchedActivity != null;
    }

    private class ActivityExistsCondition extends InstrumentationThreadCondition {
        private ActivityT mMatchedActivity;

        @Override
        protected ConditionStatus checkWithSuppliers() {
            ActivityT candidate = null;
            List<Activity> allActivities = ApplicationStatus.getRunningActivities();
            for (Activity activity : allActivities) {
                if (mActivityClass.equals(activity.getClass())) {
                    ActivityT matched = mActivityClass.cast(activity);
                    if (candidate != null) {
                        return error("%s matched two Activities: %s, %s", this, candidate, matched);
                    }
                    candidate = matched;
                }
            }
            mMatchedActivity = candidate;
            if (mMatchedActivity == null) {
                return awaiting("No Activity with expected class");
            }

            @ActivityState int state = ApplicationStatus.getStateForActivity(mMatchedActivity);
            return fulfilledOrAwaiting(
                    state == ActivityState.RESUMED,
                    "matched: %s (state=%s)",
                    mMatchedActivity,
                    activityStateDescription(state));
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
