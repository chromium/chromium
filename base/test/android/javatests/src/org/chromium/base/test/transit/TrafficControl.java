// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.util.Pair;

import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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

    static void notifyCreatedStation(Station<?> station) {
        sAllStationNames.add(Pair.create(sCurrentTestCase, station.getName()));
    }

    static void notifyEntryPointSentinelStationCreated(EntryPointSentinelStation sentinelStation) {
        for (Station<?> station : sActiveStations) {
            // Happens when test is batched, but the Activity is not kept between tests; Public
            // Transit's Station/Facility state need to reflect that and start from a new
            // {@link EntryPointSentinelStation}.
            station.setStateTransitioningFrom();
            station.setStateFinished();
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
     * Hop off Public Transit - clear the active stations so that a subsequent test can go through
     * an entry point again on the same process.
     *
     * <p>Useful in Robolectric tests.
     */
    public static void hopOffPublicTransit() {
        sActiveStations.clear();
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
}
