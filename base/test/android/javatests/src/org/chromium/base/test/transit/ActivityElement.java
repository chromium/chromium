// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Represents an {@link Activity} that needs to exist to consider the Station active.
 *
 * <p>Subclasses are treated as a different type.
 *
 * @param <ActivityT> exact type of Activity expected
 */
@NullMarked
public class ActivityElement<ActivityT extends Activity> extends Element<ActivityT> {
    private final Class<ActivityT> mActivityClass;

    ActivityElement(Class<ActivityT> activityClass) {
        super("AE/" + activityClass.getCanonicalName());
        mActivityClass = activityClass;
    }

    @Override
    public @Nullable ConditionWithResult<ActivityT> createEnterCondition() {
        // Can be overridden with requireToBeInSameTask() or requireToBeInNewTask().
        return new ActivityExistsInNewTaskCondition();
    }

    @Override
    public @Nullable Condition createExitCondition() {
        return null;
    }

    void requireToBeInSameTask(Activity activity) {
        replaceEnterCondition(new ActivityExistsInSameTaskCondition(activity));
    }

    void requireToBeInNewTask() {
        replaceEnterCondition(new ActivityExistsInNewTaskCondition());
    }

    void requireNoParticularTask() {
        replaceEnterCondition(new ActivityExistsInAnyTaskCondition());
    }

    private abstract class ActivityExistsCondition extends ConditionWithResult<ActivityT> {
        private ActivityExistsCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatusWithResult<ActivityT> resolveWithSuppliers() {
            ActivityT candidateMatchingClass = null;
            ActivityT candidateMatchingClassAndTask = null;
            String reasonForTaskIdDifference = "";
            List<Activity> allActivities = ApplicationStatus.getRunningActivities();
            for (Activity activity : allActivities) {
                if (mActivityClass.equals(activity.getClass())) {
                    ActivityT matched = mActivityClass.cast(activity);
                    candidateMatchingClass = matched;
                    reasonForTaskIdDifference = getReasonForTaskIdDifference(matched);
                    if (reasonForTaskIdDifference != null) {
                        continue;
                    }
                    if (candidateMatchingClassAndTask != null) {
                        return error(
                                        "%s matched two Activities: %s, %s",
                                        this, candidateMatchingClassAndTask, matched)
                                .withoutResult();
                    }
                    candidateMatchingClassAndTask = matched;
                }
            }
            if (candidateMatchingClass == null) {
                return awaiting("No Activity with expected class").withoutResult();
            }
            if (candidateMatchingClassAndTask == null) {
                return awaiting("Activity not in expected task: " + reasonForTaskIdDifference)
                        .withoutResult();
            }

            @ActivityState
            int state = ApplicationStatus.getStateForActivity(candidateMatchingClassAndTask);
            String statusString =
                    String.format(
                            "matched: %s (state=%s)",
                            candidateMatchingClassAndTask, activityStateDescription(state));
            if (state == ActivityState.RESUMED) {
                return fulfilled(statusString).withResult(candidateMatchingClassAndTask);
            } else {
                return awaiting(statusString).withoutResult();
            }
        }

        /**
         * Return null if |activity| is in the expected task according to the Condition's specific
         * criteria, or the reason for the difference otherwise.
         */
        protected abstract @Nullable String getReasonForTaskIdDifference(ActivityT activity);

        @Override
        public String buildDescription() {
            return "Activity exists and is RESUMED: " + mActivityClass.getSimpleName();
        }
    }

    private class ActivityExistsInAnyTaskCondition extends ActivityExistsCondition {
        @Override
        protected @Nullable String getReasonForTaskIdDifference(ActivityT activity) {
            return null;
        }

        @Override
        public String buildDescription() {
            return super.buildDescription() + " in any task";
        }
    }

    private class ActivityExistsInSameTaskCondition extends ActivityExistsCondition {
        private final int mOriginTaskId;

        private ActivityExistsInSameTaskCondition(Activity originActivity) {
            super();
            mOriginTaskId = originActivity.getTaskId();
            assert mOriginTaskId != -1 : "The origin activity was not in any task";
        }

        @Override
        protected @Nullable String getReasonForTaskIdDifference(ActivityT activity) {
            // Ignore Activities in different tasks
            int activityTaskId = activity.getTaskId();
            if (activityTaskId == mOriginTaskId) {
                return null;
            } else {
                return String.format(
                        "Origin's task id: %d, candidate's was different: %d",
                        mOriginTaskId, activityTaskId);
            }
        }

        @Override
        public String buildDescription() {
            return super.buildDescription() + " in the same task as previous Station";
        }
    }

    private class ActivityExistsInNewTaskCondition extends ActivityExistsCondition {
        private final Map<Integer, Station<?>> mExistingTaskIds;

        private ActivityExistsInNewTaskCondition() {
            super();

            // Store all task ids of Activities known to Public Transit.
            mExistingTaskIds = new HashMap<>();
            for (Station<?> activeStation : TrafficControl.getActiveStations()) {
                ActivityElement<?> knownActivityElement = activeStation.getActivityElement();
                if (knownActivityElement != null) {
                    mExistingTaskIds.put(knownActivityElement.get().getTaskId(), activeStation);
                }
            }
        }

        @Override
        protected @Nullable String getReasonForTaskIdDifference(ActivityT activity) {
            // Ignore Activities in known tasks
            int candidateTaskId = activity.getTaskId();
            Station<?> stationInSameTask = mExistingTaskIds.get(candidateTaskId);
            if (stationInSameTask != null) {
                return String.format(
                        "%s's Activity was in same task: %d",
                        stationInSameTask.getName(), candidateTaskId);
            }
            return null;
        }

        @Override
        public String buildDescription() {
            return super.buildDescription() + " in a new task";
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
