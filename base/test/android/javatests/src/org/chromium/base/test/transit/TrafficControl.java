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
    private static final List<Pair<String, Station<?>>> sAllStations = new ArrayList<>();
    private static @Nullable String sCurrentTestCase;

    private static @Nullable Station<?> sActiveStation;

    static void notifyCreatedStation(Station<?> station) {
        sAllStations.add(Pair.create(sCurrentTestCase, station));
    }

    static void notifyEntryPointSentinelStationCreated(EntryPointSentinelStation sentinelStation) {
        if (sActiveStation != null) {
            // Happens when test is batched, but the Activity is not kept between tests; Public
            // Transit's Station/Facility state need to reflect that and start from a new
            // {@link EntryPointSentinelStation}.
            sActiveStation.setStateTransitioningFrom();
            sActiveStation.setStateFinished();
        }
        sActiveStation = sentinelStation;
    }

    static void notifyActiveStationChanged(Station<?> newActiveStation) {
        assert newActiveStation.getPhase() == Phase.ACTIVE : "New active Station must be ACTIVE";
        if (sActiveStation != null) {
            assert sActiveStation.getPhase() != Phase.ACTIVE
                    : "Previously active station was not ACTIVE";
        }
        sActiveStation = newActiveStation;
    }

    public static List<Pair<String, Station<?>>> getAllStations() {
        return sAllStations;
    }

    public static @Nullable Station<?> getActiveStation() {
        return sActiveStation;
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
