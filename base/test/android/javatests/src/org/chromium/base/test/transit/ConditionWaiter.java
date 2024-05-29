// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.transit.StatusStore.StatusRegion;
import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Polls multiple {@link Condition}s in parallel. */
public class ConditionWaiter {

    /**
     * The process of waiting for a {@link Condition} to be fulfilled.
     *
     * <p>Tracks the {@link ConditionStatus}es returned over time, how long it took to be fulfilled
     * (or for long it was checked until it timed out).
     *
     * <p>Tracks and aggregates the ConditionStatues for user-friendly printing.
     */
    protected static class ConditionWait {

        private final Condition mCondition;
        private final @ConditionOrigin int mOrigin;
        private long mTimeStarted;
        private long mTimeUnfulfilled;
        private long mTimeFulfilled;
        private StatusStore mStatusStore = new StatusStore();

        /**
         * Constructor.
         *
         * @param condition the {@link Condition} that this will wait for.
         * @param origin the origin of the |condition|.
         */
        ConditionWait(Condition condition, @ConditionOrigin int origin) {
            mCondition = condition;
            mOrigin = origin;
        }

        Condition getCondition() {
            return mCondition;
        }

        @ConditionOrigin
        int getOrigin() {
            return mOrigin;
        }

        private void startTimer() {
            if (mTimeStarted > 0) {
                return;
            }

            mTimeStarted = getNow();
            mTimeUnfulfilled = mTimeStarted;
        }

        private boolean update() {
            ConditionStatus status;
            try {
                if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/40284026): Post multiple checks in parallel, the UI thread
                    // will
                    // run them sequentially.
                    status = ThreadUtils.runOnUiThreadBlocking(mCondition::check);
                } else {
                    status = mCondition.check();
                }
            } catch (Exception e) {
                StringWriter sw = new StringWriter();
                e.printStackTrace(new PrintWriter(sw));
                status = Condition.error(sw.toString());
            }

            mStatusStore.report(status);
            if (status.isError()) {
                return true;
            } else if (status.isFulfilled()) {
                reportFulfilledWait(status);
                return false;
            } else {
                reportUnfulfilledWait(status);
                return true;
            }
        }

        /** Report that the Condition being waited on is not fulfilled at this time. */
        private void reportUnfulfilledWait(ConditionStatus status) throws IllegalStateException {
            mTimeFulfilled = 0;
            mTimeUnfulfilled = status.getTimestamp();
        }

        /** Report that the Condition being waited on is fulfilled at this time. */
        private void reportFulfilledWait(ConditionStatus status) {
            if (!isFulfilled()) {
                // isFulfilled() will return true after setting a non-zero time.
                mTimeFulfilled = status.getTimestamp();
            }
        }

        /**
         * @return if the Condition is fulfilled.
         */
        private boolean isFulfilled() {
            return mTimeFulfilled > 0;
        }

        /**
         * @return how long the condition has been considered unfulfilled for.
         *     <p>The Condition must be unfulfilled, or an assertion will be raised.
         */
        private long getTimeUnfulfilled() {
            assert !isFulfilled();

            return mTimeUnfulfilled - mTimeStarted;
        }

        /**
         * @return how long the condition took to be fulfilled for the first time. The result is a
         *     pair (lowerBound, upperBound), where the time it took is between these two numbers.
         *     |lowerBound| is the last time at which the Condition was seen as unfulfilled and
         *     |upperBound| is the first time at which the Condition was seen as fulfilled.
         *     <p>The Condition must be fulfilled, or an assertion will be raised.
         */
        private Pair<Long, Long> getTimeToFulfill() {
            assert isFulfilled();

            long minTimeToFulfill = mTimeUnfulfilled - mTimeStarted;
            long maxTimeToFulfill = mTimeFulfilled - mTimeStarted;
            return Pair.create(minTimeToFulfill, maxTimeToFulfill);
        }

        private static long getNow() {
            long now = TimeUtils.currentTimeMillis();
            assert now > 0;
            return now;
        }

        /**
         * @return an aggregation of the statuses reported while checking a Condition.
         */
        public StatusStore getStatusStore() {
            return mStatusStore;
        }
    }

    /** The maximum time to wait for a criteria to become valid. */
    public static final long MAX_TIME_TO_POLL = 5000L;

    /** The polling interval to wait between checking for a satisfied criteria. */
    public static final long POLLING_INTERVAL = 50;

    private static final String TAG = "Transit";

    /**
     * Start timers, perform the first Condition checks before running the Trigger.
     *
     * <p>Ensure at least one Condition is not fulfilled before running the Trigger.
     *
     * <p>This also makes supplied values available for Conditions that implement Supplier before
     * {@link Condition#onStartMonitoring()} is called.
     */
    static void preCheck(
            List<ConditionWait> conditionWaits, TransitionOptions options, Trigger trigger) {
        if (conditionWaits.isEmpty()) {
            Log.i(TAG, "No conditions to fulfill.");
        }

        for (ConditionWait wait : conditionWaits) {
            wait.startTimer();
        }

        boolean anyCriteriaMissing = false;
        for (ConditionWait wait : conditionWaits) {
            anyCriteriaMissing |= wait.update();
        }

        // At least one Condition should be not fulfilled, or this is likely an incorrectly designed
        // Transition. Exceptions to this rule:
        //     1. null Trigger, for example when focusing on secondary elements of a screen that
        //        aren't declared in Station#declareElements().
        //     2. A explicit exception is made with TransitionOptions.mPossiblyAlreadyFulfilled.
        //        E.g. when not possible to determine whether the trigger needs to be run.
        if (!anyCriteriaMissing && !options.mPossiblyAlreadyFulfilled && trigger != null) {
            throw buildWaitConditionsException(
                    "All Conditions already fulfilled before running Trigger", conditionWaits);
        }
    }

    /**
     * Blocks waiting for multiple {@link Condition}s, polling them and reporting their status to he
     * {@link ConditionWait}es.
     *
     * @param conditionWaits the {@link ConditionWait}es to process.
     * @param transitionName String representing the Transition to print
     * @param options the {@link TransitionOptions} to configure the polling parameters.
     * @throws AssertionError if not all {@link Condition}s are fulfilled before timing out.
     */
    static void waitFor(
            List<ConditionWait> conditionWaits, String transitionName, TransitionOptions options) {
        Runnable checker =
                () -> {
                    boolean anyCriteriaMissing = false;
                    for (ConditionWait wait : conditionWaits) {
                        anyCriteriaMissing |= wait.update();
                    }

                    if (anyCriteriaMissing) {
                        throw buildWaitConditionsException(
                                "Did not meet all conditions", conditionWaits);
                    } else {
                        Log.i(
                                TAG,
                                "%s: Conditions fulfilled:\n%s",
                                transitionName,
                                createWaitConditionsSummary(conditionWaits));
                    }
                };

        long timeoutMs = options.mTimeoutMs != 0 ? options.mTimeoutMs : MAX_TIME_TO_POLL;
        CriteriaHelper.pollInstrumentationThread(checker, timeoutMs, POLLING_INTERVAL);
    }

    private static CriteriaNotSatisfiedException buildWaitConditionsException(
            String message, List<ConditionWait> conditionWaits) {
        return new CriteriaNotSatisfiedException(
                message + ":\n" + createWaitConditionsSummary(conditionWaits));
    }

    private static String createWaitConditionsSummary(List<ConditionWait> conditionStatuses) {
        StringBuilder detailsString = new StringBuilder();

        int i = 1;
        for (ConditionWait conditionStatus : conditionStatuses) {
            String conditionDescription = conditionStatus.mCondition.getDescription();

            String originString = "";
            switch (conditionStatus.mOrigin) {
                case ConditionOrigin.ENTER:
                    originString = "[ENTER]";
                    break;
                case ConditionOrigin.EXIT:
                    originString = "[EXIT ]";
                    break;
                case ConditionOrigin.TRANSITION:
                    originString = "[TRSTN]";
                    break;
            }

            String verdictString;
            if (conditionStatus.getStatusStore().anyErrorsReported()) {
                if (conditionStatus.isFulfilled()) {
                    verdictString = "[OK* ]";
                } else {
                    verdictString = "[ERR*]";
                }
            } else {
                if (conditionStatus.isFulfilled()) {
                    verdictString = "[OK  ]";
                } else {
                    verdictString = "[FAIL]";
                }
            }

            StringBuilder historyString = new StringBuilder();
            if (conditionStatus.getStatusStore().shouldPrintRegions()) {
                List<StatusRegion> statusRegions =
                        conditionStatus.getStatusStore().getStatusRegions();
                for (StatusRegion r : statusRegions) {
                    historyString.append("\n        ");
                    historyString.append(r.getLogString(conditionStatus.mTimeStarted));
                }
            }

            String fulfilledString;
            if (conditionStatus.isFulfilled()) {
                Pair<Long, Long> timeToFulfill = conditionStatus.getTimeToFulfill();
                fulfilledString =
                        String.format(
                                "{fulfilled after %d~%d ms}",
                                timeToFulfill.first, timeToFulfill.second);
            } else {
                fulfilledString =
                        String.format(
                                "{unfulfilled after %d ms}", conditionStatus.getTimeUnfulfilled());
            }

            detailsString
                    .append("    [")
                    .append(i)
                    .append("] ")
                    .append(originString)
                    .append(" ")
                    .append(verdictString)
                    .append(" ")
                    .append(conditionDescription)
                    .append(" ")
                    .append(fulfilledString);
            if (historyString.length() > 0) {
                detailsString.append(historyString);
            }
            detailsString.append('\n');
            i++;
        }
        return detailsString.toString();
    }

    /** The origin of a {@link Condition} (enter, exit, transition). */
    @IntDef({
        ConditionWaiter.ConditionOrigin.ENTER,
        ConditionWaiter.ConditionOrigin.EXIT,
        ConditionWaiter.ConditionOrigin.TRANSITION
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ConditionOrigin {
        int ENTER = 0;
        int EXIT = 1;
        int TRANSITION = 2;
    }
}
