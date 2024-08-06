// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.atomic.AtomicReference;

/** Unit Tests for {@link Trip}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TripUnitTest {

    public static class NestedFactoryStation extends Station {
        public final Condition mOuterCondition;
        public final Condition mInnerCondition;
        public final CallbackHelper mDeclareElementsCallbackHelper = new CallbackHelper();
        public final CallbackHelper mOuterCallbackHelper = new CallbackHelper();
        public final CallbackHelper mInnerCallbackHelper = new CallbackHelper();

        public NestedFactoryStation(Condition outerCondition, Condition innerCondition) {
            mOuterCondition = outerCondition;
            mInnerCondition = innerCondition;
        }

        @Override
        public void declareElements(Elements.Builder elements) {
            elements.declareLogicalElement(
                    LogicalElement.instrumentationThreadLogicalElement(
                            "LogicalElement 1, always True", () -> Condition.fulfilled()));
            elements.declareEnterCondition(
                    InstrumentationThreadCondition.from(
                            "Enter Condition 1, always True", () -> Condition.fulfilled()));
            elements.declareExitCondition(
                    InstrumentationThreadCondition.from(
                            "Exit Condition 1, always True", () -> Condition.fulfilled()));
            elements.declareEnterCondition(mOuterCondition);
            elements.declareElementFactory(
                    mOuterCondition,
                    (nestedElements) -> {
                        nestedElements.declareLogicalElement(
                                LogicalElement.instrumentationThreadLogicalElement(
                                        "LogicalElement 2, always True",
                                        () -> Condition.fulfilled()));
                        nestedElements.declareEnterCondition(
                                InstrumentationThreadCondition.from(
                                        "Enter Condition 2, always True",
                                        () -> Condition.fulfilled()));
                        nestedElements.declareExitCondition(
                                InstrumentationThreadCondition.from(
                                        "Exit Condition 2, always True",
                                        () -> Condition.fulfilled()));
                        nestedElements.declareEnterCondition(mInnerCondition);
                        nestedElements.declareElementFactory(
                                mInnerCondition,
                                (nestedNestedElements) -> {
                                    nestedNestedElements.declareLogicalElement(
                                            LogicalElement.instrumentationThreadLogicalElement(
                                                    "LogicalElement 3, always True",
                                                    () -> Condition.fulfilled()));
                                    nestedNestedElements.declareEnterCondition(
                                            InstrumentationThreadCondition.from(
                                                    "Enter Condition 3, always True",
                                                    () -> Condition.fulfilled()));
                                    nestedNestedElements.declareExitCondition(
                                            InstrumentationThreadCondition.from(
                                                    "Exit Condition 3, always True",
                                                    () -> Condition.fulfilled()));
                                    mInnerCallbackHelper.notifyCalled();
                                });
                        mOuterCallbackHelper.notifyCalled();
                    });
            mDeclareElementsCallbackHelper.notifyCalled();
        }
    }

    public static class TestCondition extends InstrumentationThreadCondition {
        public ConditionStatus mConditionStatus =
                Condition.awaiting("Waiting for a call to setConditionStatus");
        private String mDescription;

        TestCondition(String description) {
            mDescription = description;
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }

        @Override
        public ConditionStatus checkWithSuppliers() {
            return mConditionStatus;
        }

        public void setConditionStatus(ConditionStatus conditionStatus) {
            mConditionStatus = conditionStatus;
        }
    }

    @Test
    public void testTransitionWithNestedElementFactory() throws Throwable {
        Condition alwaysTrueCondition =
                InstrumentationThreadCondition.from(
                        "AlwaysTrueCondition", () -> Condition.fulfilled());
        Station sourceStation = new NestedFactoryStation(alwaysTrueCondition, alwaysTrueCondition);
        sourceStation.setStateActiveWithoutTransition();

        TestCondition outerCondition = new TestCondition("outer condition");
        TestCondition innerCondition = new TestCondition("inner condition");
        NestedFactoryStation destinationStation =
                new NestedFactoryStation(outerCondition, innerCondition);

        Thread transitionThread =
                new Thread(
                        () -> {
                            sourceStation.travelToSync(destinationStation, null);
                        });
        final AtomicReference<Throwable> maybeException = new AtomicReference();

        // Exceptions in background threads are ignored by default, must set
        // UnCaughtExceptionHandler.
        transitionThread.setUncaughtExceptionHandler(
                new Thread.UncaughtExceptionHandler() {
                    @Override
                    public void uncaughtException(Thread thread, Throwable ex) {
                        maybeException.set(ex);
                    }
                });

        try {
            transitionThread.start();
            destinationStation.mDeclareElementsCallbackHelper.waitForNext();
            assertEquals(destinationStation.mDeclareElementsCallbackHelper.getCallCount(), 1);
            assertEquals(destinationStation.mOuterCallbackHelper.getCallCount(), 0);
            assertEquals(destinationStation.mInnerCallbackHelper.getCallCount(), 0);

            outerCondition.setConditionStatus(Condition.fulfilled());
            destinationStation.mOuterCallbackHelper.waitForNext();
            assertEquals(destinationStation.mDeclareElementsCallbackHelper.getCallCount(), 1);
            assertEquals(destinationStation.mOuterCallbackHelper.getCallCount(), 1);
            assertEquals(destinationStation.mInnerCallbackHelper.getCallCount(), 0);

            innerCondition.setConditionStatus(Condition.fulfilled());
            destinationStation.mInnerCallbackHelper.waitForNext();
        } finally {
            // Wait for transition to finish to ensure it succeeds.
            transitionThread.join();
            // Rethrow exceptions inside the transition thread.
            Throwable exception = maybeException.get();
            if (exception != null) {
                throw exception;
            }
        }

        // All elements from nested factories added to the destination elements.
        assertEquals(3, destinationStation.getElements().getElements().size());
        assertEquals(2, destinationStation.getElements().getElementFactories().size());
        assertEquals(5, destinationStation.getElements().getOtherEnterConditions().size());
        assertEquals(3, destinationStation.getElements().getOtherExitConditions().size());

        // Conditions started and stopped monitoring during transition.
        assertTrue(
                outerCondition.getDescription() + " has not started monitoring",
                outerCondition.mHasStartedMonitoringForTesting);
        assertTrue(
                outerCondition.getDescription() + " has not stopped monitoring",
                outerCondition.mHasStoppedMonitoringForTesting);
        assertTrue(
                innerCondition.getDescription() + " has not started monitoring",
                innerCondition.mHasStartedMonitoringForTesting);
        assertTrue(
                innerCondition.getDescription() + " has not stopped monitoring",
                innerCondition.mHasStoppedMonitoringForTesting);

        // Factory delayed declarations should only be called once.
        assertEquals(destinationStation.mDeclareElementsCallbackHelper.getCallCount(), 1);
        assertEquals(destinationStation.mOuterCallbackHelper.getCallCount(), 1);
        assertEquals(destinationStation.mInnerCallbackHelper.getCallCount(), 1);
    }
}
