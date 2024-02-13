// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.util.ArrayMap;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;

/** Waits for multiple {@link ConditionWaitStatus}es, polling the {@link Condition}s in parallel. */
public class ConditionWaiter {

    /**
     * The fulfillment status of a {@link Condition} being waited for.
     *
     * <p>Tracks the times at which the Condition was checked to provide information about how long
     * it took to be fulfilled (or for long it was checked until it timed out).
     *
     * <p>Tracks and aggregates errors thrown during the Condition checking for user-friendly
     * printing.
     */
    static class ConditionWaitStatus {

        private final Condition mCondition;
        private final @ConditionOrigin int mOrigin;
        private long mTimeStarted;
        private long mTimeUnfulfilled;
        private long mTimeFulfilled;
        private ArrayMap<String, Integer> mErrors = new ArrayMap<>();

        /**
         * Constructor.
         *
         * @param condition the {@link Condition} that this will hold the status for.
         * @param origin the origin of the |condition|.
         */
        ConditionWaitStatus(Condition condition, @ConditionOrigin int origin) {
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
            mTimeStarted = getNow();
            mTimeUnfulfilled = mTimeStarted;
        }

        private boolean update() {
            try {
                boolean fulfilled;
                if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/1489445): Post multiple checks in parallel, the UI thread will
                    // run them sequentially.
                    fulfilled = ThreadUtils.runOnUiThreadBlocking(mCondition::check);
                } else {
                    fulfilled = mCondition.check();
                }

                if (fulfilled) {
                    reportFulfilledWait();
                    return false;
                } else {
                    reportUnfulfilledWait();
                    return true;
                }
            } catch (Exception e) {
                reportError(e.getMessage());
                return true;
            }
        }

        /**
         * Report that the Condition being waited on is not fulfilled at this time.
         *
         * @throws IllegalStateException when the Condition is unfulfilled but it had previously
         *     been fulfilled.
         */
        private void reportUnfulfilledWait() throws IllegalStateException {
            if (isFulfilled()) {
                throw new IllegalStateException("Unfulfilled after already being fulfilled");
            }

            mTimeUnfulfilled = getNow();
        }

        /** Report that the Condition being waited on is fulfilled at this time. */
        private void reportFulfilledWait() {
            if (!isFulfilled()) {
                // isFulfilled() will return true after setting a non-zero time.
                mTimeFulfilled = getNow();
            }
        }

        /**
         * Report that an error happened when checking the Condition.
         *
         * @param reason a String that will be printed as the reason; errors with the exact same
         *     reason are aggregated.
         */
        private void reportError(String reason) {
            int beforeCount = mErrors.getOrDefault(reason, 0);
            mErrors.put(reason, beforeCount + 1);
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

        /**
         * @return an aggegation of the errors reported while checking a Condition or reporting its
         *     status.
         */
        private Map<String, Integer> getErrors() {
            return mErrors;
        }

        private static long getNow() {
            long now = TimeUtils.currentTimeMillis();
            assert now > 0;
            return now;
        }
    }

    /** The maximum time to wait for a criteria to become valid. */
    public static final long MAX_TIME_TO_POLL = 5000L;

    /** The polling interval to wait between checking for a satisfied criteria. */
    public static final long POLLING_INTERVAL = 50;

    private static final String TAG = "Transit";

    /**
     * Blocks waiting for multiple {@link ConditionWaitStatus}es, polling the {@link Condition}s in
     * parallel and reporting their status to the {@link ConditionWaitStatus}es.
     *
     * <p>The timeout is |MAX_TIME_TO_POLL|.
     *
     * <p>TODO(crbug.com/1489462): Make the timeout configurable per transition.
     *
     * @param conditionStatuses the {@link ConditionWaitStatus}es to wait for.
     * @throws AssertionError if not all {@link Condition}s are fulfilled before timing out.
     */
    public static void waitFor(List<ConditionWaitStatus> conditionStatuses) {
        if (conditionStatuses.isEmpty()) {
            Log.i(TAG, "No conditions to fulfill.");
        }

        for (ConditionWaitStatus status : conditionStatuses) {
            status.startTimer();
        }

        Runnable checker =
                () -> {
                    boolean anyCriteriaMissing = false;
                    for (ConditionWaitStatus status : conditionStatuses) {
                        anyCriteriaMissing |= status.update();
                    }

                    if (anyCriteriaMissing) {
                        throw buildWaitConditionsException(conditionStatuses);
                    } else {
                        Log.i(
                                TAG,
                                "Conditions fulfilled:\n%s",
                                createWaitConditionsSummary(conditionStatuses));
                    }
                };

        CriteriaHelper.pollInstrumentationThread(checker, MAX_TIME_TO_POLL, POLLING_INTERVAL);
    }

    private static CriteriaNotSatisfiedException buildWaitConditionsException(
            List<ConditionWaitStatus> conditionStatuses) {
        return new CriteriaNotSatisfiedException(
                "Did not meet all conditions:\n" + createWaitConditionsSummary(conditionStatuses));
    }

    private static String createWaitConditionsSummary(List<ConditionWaitStatus> conditionStatuses) {
        StringBuilder detailsString = new StringBuilder();

        int i = 1;
        for (ConditionWaitStatus conditionStatus : conditionStatuses) {
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

            Map<String, Integer> errors = conditionStatus.getErrors();
            StringBuilder errorsString = new StringBuilder();
            String statusString;
            if (!errors.isEmpty()) {
                errorsString.append(" {errors: ");
                for (Map.Entry<String, Integer> e : errors.entrySet()) {
                    String errorReason = e.getKey();
                    Integer errorCount = e.getValue();
                    errorsString.append(String.format("%s (%d errors);", errorReason, errorCount));
                }
                errorsString.append("}");
                statusString = "[ERR ]";
            } else if (conditionStatus.isFulfilled()) {
                statusString = "[OK  ]";
            } else {
                statusString = "[FAIL]";
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
                    .append(statusString)
                    .append(" ")
                    .append(conditionDescription)
                    .append(" ")
                    .append(fulfilledString);
            if (errorsString.length() > 0) {
                detailsString.append(" ").append(errorsString);
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
