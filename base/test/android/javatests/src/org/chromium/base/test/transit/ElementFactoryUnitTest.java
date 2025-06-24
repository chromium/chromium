// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.atomic.AtomicReference;

/** Unit Tests for {@link ElementFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ElementFactoryUnitTest {

    private static final String TAG = "TransitUnitTest";

    private static class NestedFactoryStation extends Station<Activity> {
        public final CallbackHelper mDeclareElementsCallbackHelper = new CallbackHelper();
        public final CallbackHelper mOuterCallbackHelper = new CallbackHelper();
        public final CallbackHelper mInnerCallbackHelper = new CallbackHelper();
        public final Element<String> outerElement;
        public Element<String> innerElement;

        public NestedFactoryStation(
                ConditionWithResult<String> outerCondition,
                ConditionWithResult<String> innerCondition) {
            super(null);

            declareElement(
                    LogicalElement.instrumentationThreadLogicalElement(
                            "LogicalElement 1, always True", () -> Condition.fulfilled()));
            declareEnterCondition(
                    InstrumentationThreadCondition.from(
                            "Enter Condition 1, always True", () -> Condition.fulfilled()));
            declareExitCondition(
                    InstrumentationThreadCondition.from(
                            "Exit Condition 1, always True", () -> Condition.fulfilled()));
            outerElement = declareEnterConditionAsElement(outerCondition);
            declareElementFactory(
                    outerElement,
                    (nestedElements) -> {
                        nestedElements.declareElement(
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
                        innerElement =
                                nestedElements.declareEnterConditionAsElement(innerCondition);
                        nestedElements.declareElementFactory(
                                innerElement,
                                (nestedNestedElements) -> {
                                    nestedNestedElements.declareElement(
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

    public static class TestCondition extends ConditionWithResult<String> {
        public ConditionStatus mConditionStatus =
                Condition.awaiting("Waiting for a call to setConditionStatus");
        private final String mDescription;

        TestCondition(String description) {
            super(/* isRunOnUiThread= */ false);
            mDescription = description;
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }

        @Override
        public ConditionStatusWithResult<String> resolveWithSuppliers() {
            if (mConditionStatus.isFulfilled()) {
                return mConditionStatus.withResult("TestCondition's result");
            } else {
                return mConditionStatus.withoutResult();
            }
        }

        public void setConditionStatus(ConditionStatus conditionStatus) {
            mConditionStatus = conditionStatus;
        }
    }

    @After
    public void tearDown() {
        TrafficControl.hopOffPublicTransit();
    }

    @Test
    public void testTransitionWithNestedElementFactory() throws Throwable {
        TestCondition outerCondition = new TestCondition("outer condition");
        TestCondition innerCondition = new TestCondition("inner condition");
        NestedFactoryStation destinationStation =
                new NestedFactoryStation(outerCondition, innerCondition);

        doTestTransitionWithNestedElementFactory(
                destinationStation, outerCondition, innerCondition);
    }

    @Test
    public void testTransitionWithNestedElementFactory_replaceCondition() throws Throwable {
        TestCondition alwaysFalseCondition = new TestCondition("always false condition");
        TestCondition outerCondition = new TestCondition("outer condition");
        TestCondition innerCondition = new TestCondition("inner condition");
        NestedFactoryStation destinationStation =
                new NestedFactoryStation(alwaysFalseCondition, innerCondition);
        destinationStation.outerElement.replaceEnterCondition(outerCondition);

        doTestTransitionWithNestedElementFactory(
                destinationStation, outerCondition, innerCondition);
    }

    private void doTestTransitionWithNestedElementFactory(
            NestedFactoryStation destinationStation,
            TestCondition outerCondition,
            TestCondition innerCondition)
            throws Throwable {
        TestCondition alwaysTrueCondition = new TestCondition("always true condition");
        alwaysTrueCondition.setConditionStatus(Condition.fulfilled());
        NestedFactoryStation sourceStation =
                new NestedFactoryStation(alwaysTrueCondition, alwaysTrueCondition);
        sourceStation.setStateActiveWithoutTransition();

        Thread transitionThread =
                new Thread(
                        () -> {
                            Triggers.noopTo()
                                    .withContext(sourceStation)
                                    .arriveAt(destinationStation);
                        });
        final AtomicReference<Throwable> maybeException = new AtomicReference();

        // Exceptions in background threads are ignored by default, must set
        // UnCaughtExceptionHandler.
        transitionThread.setUncaughtExceptionHandler(
                new Thread.UncaughtExceptionHandler() {
                    @Override
                    public void uncaughtException(Thread thread, Throwable ex) {
                        Log.e(TAG, "uncaughtException ", ex);
                        maybeException.set(ex);
                    }
                });

        try {
            transitionThread.start();
            destinationStation.mDeclareElementsCallbackHelper.waitForNext();
            assertEquals(1, destinationStation.mDeclareElementsCallbackHelper.getCallCount());
            assertEquals(0, destinationStation.mOuterCallbackHelper.getCallCount());
            assertEquals(0, destinationStation.mInnerCallbackHelper.getCallCount());

            outerCondition.setConditionStatus(Condition.fulfilled());
            destinationStation.mOuterCallbackHelper.waitForNext();
            assertEquals(1, destinationStation.mDeclareElementsCallbackHelper.getCallCount());
            assertEquals(1, destinationStation.mOuterCallbackHelper.getCallCount());
            assertEquals(0, destinationStation.mInnerCallbackHelper.getCallCount());

            innerCondition.setConditionStatus(Condition.fulfilled());
            destinationStation.mInnerCallbackHelper.waitForNext();
        } finally {
            // Wait for transition to finish to ensure it succeeds.
            transitionThread.join();
        }
        // Rethrow exceptions inside the transition thread.
        Throwable exception = maybeException.get();
        if (exception != null) {
            throw exception;
        }

        // All elements from nested factories added to the destination elements.

        // 3 LogicalElements: constructor, outer factory, inner factory
        // +2 outerElement and innerElement
        assertEquals(5, destinationStation.getElements().getElements().size());
        // 2 factories: outer factory, inner factory
        assertEquals(2, destinationStation.getElements().getElementFactories().size());
        // 3 enter Conditions: constructor, outer factory, inner factory
        assertEquals(3, destinationStation.getElements().getOtherEnterConditions().size());
        // 3 exit Conditions: constructor, outer factory, inner factory
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
        assertEquals(1, destinationStation.mDeclareElementsCallbackHelper.getCallCount());
        assertEquals(1, destinationStation.mOuterCallbackHelper.getCallCount());
        assertEquals(1, destinationStation.mInnerCallbackHelper.getCallCount());
    }
}
