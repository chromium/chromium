// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Represents an {@link Activity} that needs to exist to consider the Station active.
 *
 * @param <ActivityT> type of Activity expected
 */
@NullMarked
public class ActivityElement<ActivityT extends Activity> extends Element<ActivityT> {
    /**
     * Timeout to wait for an Activity to be launched in a new task.
     *
     * <p>Starting a new task (a cold start, or a new window in multi-window) is much more expensive
     * than an in-task transition, and all Conditions in a Transition share a single timeout budget.
     * With the default budget, a slow launch starves the Conditions that wait for the new window's
     * contents, failing the Transition even though the UI settled promptly once it appeared.
     *
     * <p>Since the budget is the max of the Transition's and each Condition's timeout, this only
     * extends Transitions that actually wait for a new task.
     */
    public static final int NEW_TASK_TIMEOUT_MS = 10000;

    private final Class<ActivityT> mActivityClass;
    private boolean mAllowSubclasses;
    private @Nullable Activity mRequiredInstance;

    ActivityElement(Class<ActivityT> activityClass) {
        super("AE/" + activityClass.getCanonicalName());
        mActivityClass = activityClass;
    }

    /** Allow subclasses of the Activity class to match. */
    public void allowSubclasses() {
        if (mOwner != null) {
            mOwner.assertInPhase(ConditionalState.Phase.NEW);
        }
        mAllowSubclasses = true;
    }

    @Override
    public @Nullable ConditionWithResult<ActivityT> createEnterCondition() {
        // Can be overridden with requireToBeInSameTask() or requireToBeInNewTask().
        return new ActivityExistsInNewTaskCondition();
    }

    @Override
    public @Nullable Condition createExitCondition() {
        // expectActivityDestroyed() might set this afterwards.
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

    /**
     * Require the Element to resolve to this exact Activity instance.
     *
     * <p>Matching by class is ambiguous as soon as more than one window is open: two windows are
     * either two instances of the same Activity class, or a subclass such as ChromeTabbedActivity2
     * which an exact-class match rejects outright. When the caller already holds the Activity it
     * wants to act on, name it instead of searching for it.
     */
    void requireInstance(Activity activity) {
        if (mOwner != null) {
            mOwner.assertInPhase(ConditionalState.Phase.NEW);
        }
        assert mActivityClass.isInstance(activity)
                : String.format("%s is not a %s", activity, mActivityClass.getCanonicalName());
        mRequiredInstance = activity;
        replaceEnterCondition(new ActivityIsInstanceCondition());
    }

    TripBuilder bringWindowToFrontTo() {
        return Triggers.runOnUiThreadTo(
                () -> {
                    var activity = get();
                    assert activity != null;
                    ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), 0);
                });
    }

    /**
     * Expect the Activity to be destroyed unless transitioning to a ConditionalState which also has
     * this Activity.
     */
    public void expectActivityDestroyed() {
        assert mExitCondition == null
                : "Already set an exit condition: " + mExitCondition.getDescription();
        replaceExitCondition(new ActivityDestroyedCondition());
    }

    /** Returns the Activity class expected. */
    public Class<ActivityT> getActivityClass() {
        return mActivityClass;
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
                boolean matches;
                if (mRequiredInstance != null) {
                    // An explicitly named Activity is matched by identity, so neither the number
                    // of open windows nor the exact runtime class matters.
                    matches = activity == mRequiredInstance;
                } else if (mAllowSubclasses) {
                    matches = mActivityClass.isInstance(activity);
                } else {
                    matches = mActivityClass.equals(activity.getClass());
                }
                if (matches) {
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
                if (mRequiredInstance != null) {
                    return awaiting("Required instance not running: " + mRequiredInstance)
                            .withoutResult();
                }
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

    private class ActivityIsInstanceCondition extends ActivityExistsCondition {
        @Override
        protected @Nullable String getReasonForTaskIdDifference(ActivityT activity) {
            // Matched by identity in resolveWithSuppliers(); the task is whichever it is in.
            return null;
        }

        @Override
        public String buildDescription() {
            return super.buildDescription() + ", exact instance " + mRequiredInstance;
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
            withTimeout(NEW_TASK_TIMEOUT_MS);

            // Store all task ids of Activities known to Public Transit.
            mExistingTaskIds = new HashMap<>();
            for (Station<?> activeStation : TrafficControl.getActiveStations()) {
                ActivityElement<? extends Activity> knownActivityElement =
                        activeStation.getActivityElement();
                if (knownActivityElement != null) {
                    mExistingTaskIds.put(knownActivityElement.value().getTaskId(), activeStation);
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

    private static String activityStateDescription(@ActivityState Integer state) {
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

    private class ActivityDestroyedCondition extends InstrumentationThreadCondition {
        @Override
        protected ConditionStatus checkWithSuppliers() {
            ConditionWithResult<ActivityT> enterCondition = getEnterCondition();
            assert enterCondition != null
                    : "Must set up the enter condition before calling expectActivityDestroyed()";
            ActivityT activity = enterCondition.get();
            int status = ApplicationStatus.getStateForActivity(activity);
            return whetherEquals(
                    ActivityState.DESTROYED, status, ActivityElement::activityStateDescription);
        }

        @Override
        public String buildDescription() {
            return "Activity is DESTROYED: " + mActivityClass.getSimpleName();
        }
    }
}
