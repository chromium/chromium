// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.StatusStore.StatusRegion;
import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.base.test.util.TimeoutTimer;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

/** Polls multiple {@link Condition}s in parallel. */
@NullMarked
public class ConditionWaiter {
    /** Returns the current time in milliseconds, based on System.nanoTime(). */
    public static long getNow() {
        return System.nanoTime() / 1_000_000;
    }

    /** Calculate the timeout for the transition based on options and conditions. */
    public static long calculateTimeoutMs(TransitionOptions options, List<Condition> conditions) {
        boolean hasDefaultConditions = false;
        long maxExplicitTimeout = 0;
        for (Condition condition : conditions) {
            Integer timeoutMs = condition.getTimeoutMs();
            if (timeoutMs == null) {
                hasDefaultConditions = true;
            } else {
                maxExplicitTimeout = Math.max(maxExplicitTimeout, timeoutMs.longValue());
            }
        }
        long defaultTimeout = hasDefaultConditions ? MAX_TIME_TO_POLL : 0;
        long baseTimeout = options.mTimeoutMs != null ? options.mTimeoutMs : defaultTimeout;
        return Math.max(baseTimeout, maxExplicitTimeout);
    }

    /**
     * The process of waiting for a {@link Condition} to be fulfilled.
     *
     * <p>Tracks the {@link ConditionStatus}es returned over time, how long it took to be fulfilled
     * (or for long it was checked until it timed out).
     *
     * <p>Tracks and aggregates the ConditionStatues for user-friendly printing.
     */
    protected static class ConditionWait {

        /** The condition being waited for. */
        private final Condition mCondition;

        /** The origin of the condition (e.g., ENTER, EXIT). */
        private final @ConditionOrigin int mOrigin;

        /** True if this is the initial wait for the condition, false if it's a delayed wait. */
        private boolean mIsInitialWait = true;

        /** The time when we started waiting for this condition in ConditionWaiter. */
        private long mTimeStarted;

        /** The timestamp of the latest check where the condition was unfulfilled. */
        private long mTimeUnfulfilled;

        /** The timestamp of the first check where the condition was fulfilled. */
        private long mTimeFulfilled;

        /** Store of the condition's status history. */
        private final StatusStore mStatusStore = new StatusStore();

        /** True if this condition has timed out. */
        private boolean mTimedOut;

        /** The scaled timeout for this condition in milliseconds, or null if not yet calculated. */
        private @Nullable Long mScaledTimeoutMs;

        /**
         * Constructor.
         *
         * @param condition the {@link Condition} that this will wait for.
         * @param origin the origin of the |condition|.
         */
        ConditionWait(Condition condition, @ConditionOrigin int origin) {
            condition.assertIsBound();
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

        void markAsDelayedWait() {
            mIsInitialWait = false;
        }

        boolean isInitialWait() {
            return mIsInitialWait;
        }

        boolean isTimedOut() {
            return mTimedOut;
        }

        long getScaledTimeoutMs() {
            assert mScaledTimeoutMs != null;
            return mScaledTimeoutMs;
        }

        boolean hasCustomTimeout() {
            return mCondition.getTimeoutMs() != null;
        }

        /**
         * Starts the timer and calculates the scaled timeout.
         *
         * @param transitionTimeoutMs the timeout of the transition, used if the condition does not
         *     have a custom timeout.
         */
        void start(long transitionTimeoutMs) {
            ensureTimerStarted();
            calculateScaledTimeout(transitionTimeoutMs);
        }

        private void calculateScaledTimeout(long transitionTimeoutMs) {
            long conditionTimeout =
                    mCondition.getTimeoutMs() != null
                            ? mCondition.getTimeoutMs().longValue()
                            : transitionTimeoutMs;
            mScaledTimeoutMs = ScalableTimeout.scaleTimeout(conditionTimeout);
        }

        private void ensureTimerStarted() {
            if (mTimeStarted > 0) {
                return;
            }

            mTimeStarted = getNow();
            mTimeUnfulfilled = mTimeStarted;
        }

        private boolean update(boolean isPreCheck) {
            ConditionStatus status;
            try {
                if (isPreCheck && !mCondition.shouldRunInPreCheck()) {
                    status = Condition.awaiting(/* message= */ null);
                } else if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/40284026): Post multiple checks in parallel, the UI thread
                    // will run them sequentially.
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
            if (status.isFulfilled()) {
                reportFulfilledWait(status);
                return false;
            } else {
                reportUnfulfilledWait(status);
                if (!isPreCheck) {
                    if (getTimeUnfulfilled() >= getScaledTimeoutMs()) {
                        mTimedOut = true;
                    }
                }
                return !mTimedOut;
            }
        }

        /** Report that the Condition being waited on is not fulfilled at this time. */
        private void reportUnfulfilledWait(ConditionStatus status) throws IllegalStateException {
            assert mTimeStarted > 0
                    : "\"" + getCondition().getDescription() + "\"'s wait timer never started";
            mTimeFulfilled = 0;
            mTimeUnfulfilled = status.getTimestamp();
        }

        /** Report that the Condition being waited on is fulfilled at this time. */
        private void reportFulfilledWait(ConditionStatus status) {
            assert mTimeStarted > 0
                    : "\"" + getCondition().getDescription() + "\"'s wait timer never started";
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

    // Emoji make the blocks easy to tell apart when scanning; the word keeps them readable without.
    private static final String FAILED_HEADER_PREFIX = "❌ FAILED ";
    private static final String BLOCKED_HEADER_PREFIX = "⏳ BLOCKED ";
    private static final String OK_HEADER_PREFIX = "✅ OK ";

    protected final Transition mTransition;
    protected @MonotonicNonNull List<ConditionWait> mWaits;
    protected @MonotonicNonNull Map<Condition, ElementFactory> mConditionsGuardingFactories;
    protected final Map<String, ConditionWait> mExitWaitsByElementId = new HashMap<>();
    private boolean mPreCheckFulfilledConditions;
    private long mTimeoutMs;

    ConditionWaiter(Transition transition) {
        mTransition = transition;
    }

    @EnsuresNonNull({"mWaits", "mConditionsGuardingFactories"})
    protected void onBeforeTransition(boolean failOnAlreadyFulfilled) {
        preCheck(failOnAlreadyFulfilled);
        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStartMonitoring();
        }
    }

    @EnsuresNonNullIf({"mWaits", "mConditionsGuardingFactories"})
    private boolean isPreCheckDone() {
        return mWaits != null && mConditionsGuardingFactories != null;
    }

    protected void onAfterTransition() {
        assert isPreCheckDone();
        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStopMonitoring();
        }
    }

    /**
     * Start timers, perform the first Condition checks before running the Trigger.
     *
     * <p>Ensure at least one Condition is not fulfilled before running the Trigger.
     *
     * <p>This also makes supplied values available for Conditions that implement Supplier before
     * {@link Condition#onStartMonitoring()} is called.
     */
    @EnsuresNonNull({"mWaits", "mConditionsGuardingFactories"})
    void preCheck(boolean failOnAlreadyFulfilled) {
        createWaits();
        List<Condition> conditions = new ArrayList<>();
        for (ConditionWait wait : mWaits) {
            conditions.add(wait.getCondition());
        }
        mTimeoutMs = calculateTimeoutMs(mTransition.getOptions(), conditions);
        createFactories();

        if (mWaits.isEmpty()) {
            if (failOnAlreadyFulfilled) {
                throw new IllegalArgumentException(
                        "No conditions to fulfill. If this is expected, use a null Trigger.");
            } else {
                Log.i(TAG, "No conditions to fulfill.");
            }
        }

        for (ConditionWait wait : mWaits) {
            wait.start(mTimeoutMs);
        }
        processWaits(/* isPreCheck= */ true, /* timeoutTimer= */ null);
        boolean allConditionsFulfilled = areAllConditionsFulfilled();

        if (allConditionsFulfilled && failOnAlreadyFulfilled) {
            throw new CriteriaNotSatisfiedException(
                    "All Conditions already fulfilled before running Trigger. If this is expected,"
                        + " use a null Trigger. If this is possible but not necessarily expected,"
                        + " use TransitionOptions.withPossiblyAlreadyFulfilled().\n"
                            + createWaitConditionsSummary(
                                    mWaits, /* generateMainMessage= */ false));
        }

        // If the preCheck already saw all Conditions fulfilled and there is no trigger which might
        // cause state changes, avoid checking Conditions a second time.
        if (!mTransition.hasTrigger() && allConditionsFulfilled) {
            mPreCheckFulfilledConditions = true;

            Log.i(
                    TAG,
                    "%s: Conditions fulfilled in preCheck:\n%s",
                    mTransition.toDebugString(),
                    createWaitConditionsSummary(mWaits, /* generateMainMessage= */ false));
        }
    }

    /**
     * Blocks waiting for multiple {@link Condition}s, polling them and reporting their status to he
     * {@link ConditionWait}es.
     *
     * @throws TravelException if not all {@link Condition}s are fulfilled before timing out.
     */
    void waitFor() throws TravelException {
        assert !ThreadUtils.runningOnUiThread();
        assert isPreCheckDone();

        if (!mPreCheckFulfilledConditions) {
            long timeoutMs = mTimeoutMs;
            TimeoutTimer timeoutTimer = new TimeoutTimer(timeoutMs);
            while (!timeoutTimer.isTimedOut()) {
                processWaits(/* isPreCheck= */ false, timeoutTimer);
                if (shouldStopWaiting()) {
                    break;
                }
                try {
                    Thread.sleep(POLLING_INTERVAL);
                } catch (InterruptedException e) {
                    // ignore
                }
            }

            if (!shouldStopWaiting()) {
                // Run one last time after timeout to ensure the total time has been given
                // and that every Condition has been checked at least once.
                processWaits(/* isPreCheck= */ false, null);
            }

            boolean hasFailures = false;
            for (ConditionWait wait : mWaits) {
                if (!wait.isFulfilled() || wait.isTimedOut()) {
                    hasFailures = true;
                    break;
                }
            }

            if (hasFailures) {
                throw TravelException.newTravelException(
                        "Did not complete "
                                + mTransition.toDebugString()
                                + ", "
                                + createWaitConditionsSummary(
                                        mWaits, /* generateMainMessage= */ true));
            } else {
                Log.i(
                        TAG,
                        "%s: Conditions fulfilled:\n%s",
                        mTransition.toDebugString(),
                        createWaitConditionsSummary(mWaits, /* generateMainMessage= */ false));
            }
        }

        // Check that all element factories were used.
        if (!mConditionsGuardingFactories.isEmpty()) {
            StringBuilder failureMessage =
                    new StringBuilder("Some Conditions of element factories were not declared:\n");
            for (Condition condition : mConditionsGuardingFactories.keySet()) {
                failureMessage.append("  * ").append(condition.getDescription()).append("\n");
            }
            throw TravelException.newTravelException(failureMessage.toString());
        }
    }

    private boolean areAllConditionsFulfilled() {
        assumeNonNull(mWaits);
        for (ConditionWait wait : mWaits) {
            if (!wait.isFulfilled()) {
                return false;
            }
        }
        return true;
    }

    private boolean shouldStopWaiting() {
        assumeNonNull(mWaits);
        for (ConditionWait wait : mWaits) {
            if (!wait.isFulfilled() && !wait.isTimedOut()) {
                return false;
            }
        }
        return true;
    }

    @EnsuresNonNull("mWaits")
    private void createWaits() {
        List<ConditionWait> allWaits = new ArrayList<>();

        Set<String> destinationElementIds = new HashSet<>();
        for (ConditionalState conditionalState : mTransition.getEnteredStates()) {
            final Elements destinationElements = conditionalState.getElements();
            List<ConditionWait> newWaits = createEnterConditionWaits(destinationElements);
            allWaits.addAll(newWaits);
            destinationElementIds.addAll(destinationElements.getElementIds());
        }

        // Create EXIT Conditions for Views that should disappear and LogicalElements that should
        // be false.
        for (ConditionalState conditionalState : mTransition.getExitedStates()) {
            final Elements originElements = conditionalState.getElements();
            for (Element<?> element : originElements.getElements()) {
                Condition exitCondition = element.getExitConditionFiltered(destinationElementIds);
                if (exitCondition != null) {
                    ConditionWait conditionWait =
                            new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT);
                    // Keep track of exit waits by element id so that any new
                    // elements added by an element factory can remove matching
                    // previously created exit wait (since the element is now a shared element).
                    mExitWaitsByElementId.put(element.getId(), conditionWait);
                    allWaits.add(conditionWait);
                }
            }

            // Add extra EXIT Conditions.
            for (Condition exitCondition : originElements.getOtherExitConditions()) {
                allWaits.add(
                        new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        // Add transition (TRSTN) conditions
        for (Condition condition : mTransition.getTransitionConditions()) {
            condition.bindToTransition(mTransition);
            allWaits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        mWaits = allWaits;
    }

    @EnsuresNonNull("mConditionsGuardingFactories")
    private void createFactories() {
        Map<Condition, ElementFactory> allConditionsGuardingFactories = new HashMap<>();

        for (ConditionalState conditionalState : mTransition.getEnteredStates()) {
            final Elements destinationElements = conditionalState.getElements();
            for (Map.Entry<Element<?>, ElementFactory> entry :
                    destinationElements.getElementFactories().entrySet()) {
                Element<?> elementToWait = entry.getKey();
                ConditionalState elementOwner = elementToWait.getOwner();
                assert elementOwner != null
                        : String.format("Element \"%s\" is not bound", elementToWait);
                int elementOwnerPhase = elementOwner.getPhase();
                ElementFactory factory = entry.getValue();
                assert elementOwnerPhase == ConditionalState.Phase.TRANSITIONING_TO
                                || elementOwnerPhase == ConditionalState.Phase.ACTIVE
                        : String.format(
                                "Cannot create ElementFactory waiting for element \"%s\" owned by"
                                        + " %s because the owner is in Phase %s",
                                elementToWait,
                                elementOwner,
                                ConditionalState.phaseToShortString(elementOwnerPhase));
                allConditionsGuardingFactories.put(
                        elementToWait.getEnterConditionChecked(), factory);
            }
        }

        mConditionsGuardingFactories = allConditionsGuardingFactories;
    }

    /**
     * Processes destination elements to get a list of waits for their enter conditions, also called
     * when processing new elements from ElementFactory.
     *
     * @param elements The elements to process (i.e. create ConditionWaits for).
     * @return the created {@link ConditionWait}s.
     */
    private List<ConditionWait> createEnterConditionWaits(BaseElements elements) {
        final List<ConditionWait> newWaits = new ArrayList<>();
        for (Element<?> element : elements.getElements()) {
            Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                newWaits.add(
                        new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        // Add extra ENTER Conditions.
        for (Condition enterCondition : elements.getOtherEnterConditions()) {
            newWaits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        return newWaits;
    }

    private void processWaits(boolean isPreCheck, @Nullable TimeoutTimer timeoutTimer) {
        assert isPreCheckDone();

        Set<String> newElementIds = new HashSet<>();

        // We process waits in batches because if a wait that guards a factory is
        // fulfilled, the new elements fabricated by the factory create new
        // waits that also need to be processed. nextBatch starts with the
        // original list of waits |mWaits| and at the end of the loop, nextBatch
        // contains any new waits created while processing the current batch.
        // We stop when no new Waits are created (i.e. nextBatch is empty).
        List<ConditionWait> nextBatch = mWaits;
        mWaits = new ArrayList<>();
        while (!nextBatch.isEmpty()) {
            List<Condition> conditionsToRemoveFromFactoryMap = new ArrayList<>();
            List<ElementFactory> factoriesReadyToFabricate = new ArrayList<>();
            for (ConditionWait wait : nextBatch) {
                // Check timeout before each Condition check; if multiple Conditions are taking
                // long, the Transition can take too long to time out.
                if (timeoutTimer != null && timeoutTimer.isTimedOut()) {
                    mWaits.addAll(nextBatch);
                    return;
                }

                boolean stillNeedsWait = wait.update(isPreCheck);
                ElementFactory generator = mConditionsGuardingFactories.get(wait.mCondition);
                // update() also returns false when the Condition times out, so check that it was
                // actually fulfilled. Fabricating elements from a Condition that never produced a
                // result makes the factory read a value-less Element, which throws and masks the
                // real reason the Transition failed.
                if (!stillNeedsWait && wait.isFulfilled() && generator != null) {
                    // Remove from the map so that next time we check this wait
                    // we dont rerun the factory.
                    conditionsToRemoveFromFactoryMap.add(wait.mCondition);
                    factoriesReadyToFabricate.add(generator);
                }
            }

            // Call factories waiting for Conditions from past transitions
            for (Map.Entry<Condition, ElementFactory> entry :
                    mConditionsGuardingFactories.entrySet()) {
                Condition conditionToWait = entry.getKey();
                ElementFactory generator = entry.getValue();

                // Already checked before adding to mConditionsGuardingFactories
                assumeNonNull(conditionToWait.mOwnerState);

                if (conditionToWait.mOwnerState.getPhase() == ConditionalState.Phase.ACTIVE) {
                    conditionsToRemoveFromFactoryMap.add(conditionToWait);
                    factoriesReadyToFabricate.add(generator);
                }
            }

            for (Condition condition : conditionsToRemoveFromFactoryMap) {
                mConditionsGuardingFactories.remove(condition);
            }

            mWaits.addAll(nextBatch);

            BaseElements newElements = fabricateElements(factoriesReadyToFabricate);
            nextBatch = createEnterConditionWaits(newElements);

            for (ConditionWait wait : nextBatch) {
                wait.markAsDelayedWait();
                wait.start(mTimeoutMs);
                // We do not want to start monitoring conditions (even newly
                // created ones) during the first update cycle (aka preCheck)
                // since we already do that after.
                if (!isPreCheck) {
                    wait.getCondition().onStartMonitoring();
                }
            }

            for (Map.Entry<Element<?>, ElementFactory> entry :
                    newElements.getElementFactories().entrySet()) {
                mConditionsGuardingFactories.put(
                        entry.getKey().getEnterConditionChecked(), entry.getValue());
            }
            newElementIds.addAll(newElements.getElementIds());
        }

        for (String elementId : newElementIds) {
            // Check if an exit condition matching the newly added element
            // exists and remove it since it is now a shared element.
            ConditionWait removedExitWait = mExitWaitsByElementId.remove(elementId);
            if (removedExitWait != null) {
                mWaits.remove(removedExitWait);
            }
        }
    }

    private BaseElements fabricateElements(List<ElementFactory> factories) {
        BaseElements newElements = new BaseElements();
        for (ElementFactory factory : factories) {
            newElements.addAll(factory.processDelayedDeclarations());
        }
        return newElements;
    }

    /**
     * Creates a summary of the status of all {@link ConditionWait}s, grouped by the
     * ConditionalState entered/exited or the Transition. Failed Conditions are listed first, then
     * Conditions blocked on inputs, then OK ones.
     */
    private static String createWaitConditionsSummary(
            List<ConditionWait> conditionWaits, boolean generateMainMessage) {
        String firstFailedConditionString = null;
        String firstBlockedConditionString = null;
        int failedConditionCount = 0;
        int blockedConditionCount = 0;
        Map<String, ConditionGroup> groups = new LinkedHashMap<>();

        // Index Conditions to reference which Condition blocks another.
        IdentityHashMap<Condition, Integer> conditionIndices = new IdentityHashMap<>();
        int i = 1;
        for (ConditionWait conditionWait : conditionWaits) {
            conditionIndices.put(conditionWait.mCondition, i);
            i++;
        }

        i = 1;
        for (ConditionWait conditionWait : conditionWaits) {
            String conditionDescription = conditionWait.mCondition.getDescription();

            String indexString = "[" + i + "]";

            // "+" marks Conditions added after the wait started.
            String delayedString = conditionWait.isInitialWait() ? " " : "+";

            boolean failed = false;
            Map<String, Supplier<?>> blockingSuppliers = null;
            String verdictString;
            if (conditionWait.isFulfilled()) {
                if (conditionWait.isTimedOut()) {
                    verdictString = "[LATE]";
                    failed = true;
                } else if (conditionWait.getStatusStore().anyErrorsReported()) {
                    verdictString = "[OK* ]";
                } else {
                    verdictString = "[OK  ]";
                }
            } else {
                blockingSuppliers = conditionWait.getStatusStore().getLastBlockingSuppliers();
                if (conditionWait.getStatusStore().anyErrorsReported()) {
                    verdictString = "[ERR*]";
                    blockingSuppliers = null;
                } else if (blockingSuppliers != null) {
                    verdictString = "[BLKD]";
                } else {
                    verdictString = "[FAIL]";
                }
                failed = true;
            }
            boolean blocked = blockingSuppliers != null;
            if (blocked) {
                if (firstBlockedConditionString == null) {
                    firstBlockedConditionString = indexString + " " + conditionDescription;
                }
                blockedConditionCount++;
            } else if (failed) {
                if (firstFailedConditionString == null) {
                    firstFailedConditionString = indexString + " " + conditionDescription;
                }
                failedConditionCount++;
            }

            StringBuilder historyString = new StringBuilder();
            // Blocked Conditions only waited for their inputs, which "blocked on" already says.
            if (!blocked && conditionWait.getStatusStore().shouldPrintRegions()) {
                List<StatusRegion> statusRegions =
                        conditionWait.getStatusStore().getStatusRegions();
                for (StatusRegion r : statusRegions) {
                    historyString.append("\n        ");
                    historyString.append(r.getLogString(conditionWait.mTimeStarted));
                }
            }

            String fulfilledString;
            if (conditionWait.isFulfilled()) {
                Pair<Long, Long> timeToFulfill = conditionWait.getTimeToFulfill();
                if (conditionWait.isTimedOut()) {
                    if (conditionWait.hasCustomTimeout()) {
                        fulfilledString =
                                String.format(
                                        "{fulfilled LATE after %d~%d ms, timeout was %d ms}",
                                        timeToFulfill.first,
                                        timeToFulfill.second,
                                        conditionWait.getScaledTimeoutMs());
                    } else {
                        fulfilledString =
                                String.format(
                                        "{fulfilled LATE after %d~%d ms}",
                                        timeToFulfill.first, timeToFulfill.second);
                    }
                } else {
                    fulfilledString =
                            String.format(
                                    "{fulfilled after %d~%d ms}",
                                    timeToFulfill.first, timeToFulfill.second);
                }
            } else {
                String unfulfilledPrefix =
                        blockingSuppliers != null
                                ? "blocked on "
                                        + describeBlockers(blockingSuppliers, conditionIndices)
                                        + " for"
                                : "unfulfilled after";
                if (conditionWait.hasCustomTimeout()) {
                    fulfilledString =
                            String.format(
                                    "{%s %d ms, timeout was %d ms}",
                                    unfulfilledPrefix,
                                    conditionWait.getTimeUnfulfilled(),
                                    conditionWait.getScaledTimeoutMs());
                } else {
                    fulfilledString =
                            String.format(
                                    "{%s %d ms}",
                                    unfulfilledPrefix, conditionWait.getTimeUnfulfilled());
                }
            }

            StringBuilder rowString = new StringBuilder();
            rowString
                    .append("   ")
                    .append(delayedString)
                    .append(indexString)
                    .append(" ")
                    .append(verdictString)
                    .append(" ")
                    .append(conditionDescription)
                    .append(" ")
                    .append(fulfilledString);
            if (historyString.length() > 0) {
                rowString.append(historyString);
            }
            rowString.append('\n');

            ConditionGroup group =
                    groups.computeIfAbsent(getGroupHeader(conditionWait), ConditionGroup::new);
            StringBuilder rows;
            if (blocked) {
                rows = group.mBlockedRows;
            } else if (failed) {
                rows = group.mFailedRows;
            } else {
                rows = group.mOkRows;
            }
            rows.append(rowString);
            i++;
        }

        StringBuilder failedBlock = new StringBuilder();
        StringBuilder blockedBlock = new StringBuilder();
        StringBuilder okBlock = new StringBuilder();
        for (ConditionGroup group : groups.values()) {
            appendGroup(failedBlock, FAILED_HEADER_PREFIX, group.mHeader, group.mFailedRows);
            appendGroup(blockedBlock, BLOCKED_HEADER_PREFIX, group.mHeader, group.mBlockedRows);
            appendGroup(okBlock, OK_HEADER_PREFIX, group.mHeader, group.mOkRows);
        }
        StringBuilder detailsString = new StringBuilder();
        for (StringBuilder block : List.of(failedBlock, blockedBlock, okBlock)) {
            if (block.length() == 0) {
                continue;
            }
            if (detailsString.length() > 0) {
                detailsString.append('\n');
            }
            detailsString.append(block);
        }

        if (generateMainMessage) {
            if (failedConditionCount == 0 && blockedConditionCount == 0) {
                return String.format("all Conditions fulfilled:\n\n%s", detailsString);
            } else if (failedConditionCount == 0) {
                // Only blocked Conditions, e.g. a Supplier from outside this Transition never
                // supplied a value.
                return String.format(
                        "%d Condition%s blocked: %s\n\n%s",
                        blockedConditionCount,
                        blockedConditionCount == 1 ? "" : "s",
                        firstBlockedConditionString,
                        detailsString);
            } else if (failedConditionCount == 1) {
                return String.format(
                        "failed 1 Condition: %s\n\n%s", firstFailedConditionString, detailsString);
            } else {
                return String.format(
                        "failed %d Conditions: %s (+%d more)\n\n%s",
                        failedConditionCount,
                        firstFailedConditionString,
                        failedConditionCount - 1,
                        detailsString);
            }
        } else {
            return detailsString.toString();
        }
    }

    private static void appendGroup(
            StringBuilder block, String prefix, String header, StringBuilder rows) {
        if (rows.length() > 0) {
            block.append(prefix).append(header).append(":\n").append(rows);
        }
    }

    /**
     * Describes the Suppliers blocking a Condition, referencing the Condition that provides each
     * Supplier's value by index when it is part of this Transition, e.g. "[5]", or by the input
     * name given to {@link Condition#dependOnSupplier(Supplier, String)} otherwise.
     */
    private static String describeBlockers(
            Map<String, Supplier<?>> blockingSuppliers,
            IdentityHashMap<Condition, Integer> conditionIndices) {
        List<String> blockers = new ArrayList<>();
        for (Map.Entry<String, Supplier<?>> entry : blockingSuppliers.entrySet()) {
            Supplier<?> supplier = entry.getValue();
            Condition producer = null;
            if (supplier instanceof Element<?> element) {
                producer = element.getEnterCondition();
            } else if (supplier instanceof Condition condition) {
                producer = condition;
            }
            Integer index = producer != null ? conditionIndices.get(producer) : null;
            blockers.add(index != null ? "[" + index + "]" : entry.getKey());
        }
        return String.join(", ", blockers);
    }

    /** Returns e.g. "ENTER <S1: SomeStation>", "EXIT <C2: SomeFacility>" or "TRANSITION". */
    private static String getGroupHeader(ConditionWait conditionWait) {
        if (conditionWait.mOrigin == ConditionOrigin.TRANSITION) {
            return "TRANSITION";
        }
        String originString = conditionWait.mOrigin == ConditionOrigin.ENTER ? "ENTER" : "EXIT";
        ConditionalState ownerState = conditionWait.mCondition.mOwnerState;
        String stateName = ownerState != null ? ownerState.getName() : "<unknown state>";
        return originString + " " + stateName;
    }

    /** Summary rows of the Conditions with the same header. */
    private static class ConditionGroup {
        final String mHeader;
        final StringBuilder mFailedRows = new StringBuilder();
        final StringBuilder mBlockedRows = new StringBuilder();
        final StringBuilder mOkRows = new StringBuilder();

        ConditionGroup(String header) {
            mHeader = header;
        }
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
