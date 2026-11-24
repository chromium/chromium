// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;

import android.app.Activity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link Outcome}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OutcomeUnitTest {
    @Rule public final PublicTransitUnitTestRule mTransitRule = new PublicTransitUnitTestRule();

    private static class TestStation extends Station<Activity> {
        TestStation() {
            super(Activity.class);
        }
    }

    private static class TestFacility extends Facility<TestStation> {}

    private TestStation hopOn() throws Throwable {
        TestStation station = new TestStation();
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTransitRule.runTransition(() -> TrafficControl.hopOnAt(activity, station));
        return station;
    }

    @Test
    public void testReach_entersAndReturnsResult() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();
        AtomicReference<TestFacility> result = new AtomicReference<>();

        mTransitRule.runTransition(
                () -> result.set(station.noopTo().reach(Outcome.enter(facility))));

        assertSame(facility, result.get());
        assertEquals(Phase.ACTIVE, facility.getPhase());
    }

    @Test
    public void testAfter_sameAsReach() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();
        AtomicReference<TestFacility> result = new AtomicReference<>();

        mTransitRule.runTransition(
                () -> result.set(Outcome.enter(facility).after(station.noopTo())));

        assertSame(facility, result.get());
        assertEquals(Phase.ACTIVE, facility.getPhase());
    }

    @Test
    public void testArriveAt_exitsOrigin() throws Throwable {
        TestStation origin = hopOn();
        TestStation destination = new TestStation();

        mTransitRule.runTransition(() -> origin.noopTo().reach(Outcome.enter(destination)));

        assertEquals(Phase.FINISHED, origin.getPhase());
        assertEquals(Phase.ACTIVE, destination.getPhase());
    }

    @Test
    public void testAndAndExit_combinedInOneTrip() throws Throwable {
        TestStation station = hopOn();
        TestFacility old = new TestFacility();
        mTransitRule.runTransition(() -> station.noopTo().reach(Outcome.enter(old)));
        TestFacility first = new TestFacility();
        TestFacility second = new TestFacility();

        mTransitRule.runTransition(
                () -> station.noopTo().reach(Outcome.enter(first).and(second).andExit(old)));

        assertEquals(Phase.FINISHED, old.getPhase());
        assertEquals(Phase.ACTIVE, first.getPhase());
        assertEquals(Phase.ACTIVE, second.getPhase());
    }

    @Test
    public void testReturning_replacesResult() throws Throwable {
        TestStation station = hopOn();
        TestFacility first = new TestFacility();
        TestFacility second = new TestFacility();
        Outcome<TestFacility> outcome = Outcome.enter(first).and(second).returning(() -> second);
        AtomicReference<TestFacility> result = new AtomicReference<>();

        mTransitRule.runTransition(() -> result.set(station.noopTo().reach(outcome)));

        assertSame(second, result.get());
        assertEquals(Phase.ACTIVE, first.getPhase());
    }

    @Test
    public void testReachTwice_asserts() throws Throwable {
        TestStation station = hopOn();
        Outcome<TestFacility> outcome = Outcome.enter(new TestFacility());
        mTransitRule.runTransition(() -> station.noopTo().reach(outcome));

        mTransitRule.runTransitionExpectingThrow(
                AssertionError.class, () -> station.noopTo().reach(outcome));
    }

    @Test
    public void testExitNotActive_asserts() throws Throwable {
        TestStation station = hopOn();
        // Building the Outcome is fine; the Facility only has to be ACTIVE when it is reached.
        Outcome<?> outcome = Outcome.exit(new TestFacility());

        mTransitRule.runTransitionExpectingThrow(
                AssertionError.class, () -> station.noopTo().reach(outcome));
    }

    @Test
    public void testAwait_noTrigger() throws Throwable {
        hopOn();
        TestFacility facility = new TestFacility();
        AtomicReference<TestFacility> result = new AtomicReference<>();

        mTransitRule.runTransition(() -> result.set(Outcome.enter(facility).await()));

        assertSame(facility, result.get());
        assertEquals(Phase.ACTIVE, facility.getPhase());
    }

    @Test
    public void testHopOnAt_entersStationWithoutOrigin() throws Throwable {
        TestStation station = new TestStation();
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();

        mTransitRule.runTransition(() -> Outcome.hopOnAt(activity, station).await());

        assertEquals(Phase.ACTIVE, station.getPhase());
    }

    @Test
    public void testInNewTask_withoutStation_asserts() {
        Outcome<TestFacility> outcome = Outcome.enter(new TestFacility());

        assertThrows(
                AssertionError.class,
                () -> {
                    var _ = outcome.inNewTask();
                });
    }
}
