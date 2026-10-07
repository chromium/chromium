// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.util.Pair;

import org.chromium.base.test.BaseJUnit4ClassRunner.ClassCleanupHook;
import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

import java.util.ArrayList;
import java.util.List;

/**
 * Keeps track of all existing {@link Station}s and which one is active.
 *
 * <p>Also keeps track of which test is currently running for batched tests.
 */
@NullMarked
public class TrafficControl {

    private static final List<Pair<String, String>> sAllStationNames = new ArrayList<>();
    private static @Nullable String sCurrentTestCase;

    private static final List<Station<?>> sActiveStations = new ArrayList<>();
    private static final List<Runnable> sHopOffListeners = new ArrayList<>();

    public static void addHopOffListener(Runnable listener) {
        sHopOffListeners.add(listener);
    }

    static void notifyCreatedStation(Station<?> station) {
        sAllStationNames.add(Pair.create(sCurrentTestCase, station.getName()));
    }

    static void notifyEntryPointSentinelStationCreated(EntryPointSentinelStation sentinelStation) {
        for (Station<?> station : sActiveStations) {
            // Happens when test is batched, but the Activity is not kept between tests; Public
            // Transit's Station/Facility state need to reflect that and start from a new
            // {@link EntryPointSentinelStation}.
            station.finishForcibly();
        }
        sActiveStations.clear();
    }

    static void notifyActiveStationsChanged(
            List<Station<?>> exitedStations, List<Station<?>> enteredStations) {
        for (Station<?> enteredStation : enteredStations) {
            assert enteredStation.getPhase() == Phase.ACTIVE : "New active Station must be ACTIVE";
        }
        for (Station<?> exitedStation : exitedStations) {
            assert exitedStation.getPhase() != Phase.ACTIVE
                    : "Previously active station was not ACTIVE";
        }
        sActiveStations.removeAll(exitedStations);
        sActiveStations.addAll(enteredStations);
    }

    /**
     * Hop off Public Transit - abandon the active Stations so that a subsequent test, or a section
     * of a test that does not use Public Transit, can go through an entry point again on the same
     * process.
     *
     * <p>Exit Conditions are deliberately *not* checked. By the time a test hops off, it is about
     * to do something Public Transit cannot see, so there is nothing trustworthy to verify. Use
     * {@link TripBuilder#reachLastStop()} instead when the test is in a known state and the exit
     * Conditions should be verified.
     *
     * <p>The Stations hopped off from, and their Facilities, are moved to {@link Phase#FINISHED},
     * so that using their Elements afterwards fails loudly instead of returning stale objects. This
     * is done from whatever phase they are in: a Station left in TRANSITIONING_FROM by a failed
     * Transition is exactly the kind of state hopping off exists to abandon, and asserting on it
     * would turn one failed test into a failed batch.
     */
    public static void hopOffPublicTransit() {
        for (Station<?> station : sActiveStations) {
            station.finishForcibly();
        }
        sActiveStations.clear();
        for (Runnable listener : sHopOffListeners) {
            listener.run();
        }
    }

    /**
     * Hop on Public Transit at |destination| in |activity|, without performing any action.
     *
     * <p>For test utils whose UI is already in the state |destination| models, which is the common
     * case. Shorthand for {@code Triggers.noopTo().hopOnTo(activity, destination, facilities)}; use
     * {@link TripBuilder#hopOnTo(Activity, Station, Facility[])} with a trigger to hop on as the
     * result of an action.
     *
     * @return |destination|, now ACTIVE.
     */
    public static <T extends Station<?>> T hopOnAt(
            Activity activity, T destination, Facility<?>... facilities) {
        return Triggers.noopTo().hopOnTo(activity, destination, facilities);
    }

    public static List<Pair<String, String>> getAllStationsNames() {
        return sAllStationNames;
    }

    public static List<Station<?>> getActiveStations() {
        return sActiveStations;
    }

    static void onTestStarted(String testName) {
        sCurrentTestCase = testName;
    }

    static void onTestFinished(String testName) {
        sCurrentTestCase = null;
    }

    static @Nullable String getCurrentTestCase() {
        return sCurrentTestCase;
    }

    /**
     * {@link ClassCleanupHook} implementation for TrafficControl. ServiceImpl makes it so that this
     * is called after every test class.
     */
    @ServiceImpl(ClassCleanupHook.class)
    public static class CleanupHook implements ClassCleanupHook {
        @Override
        public void onAfterTestClass(Class<?> clazz) {
            hopOffPublicTransit();
        }
    }
}
