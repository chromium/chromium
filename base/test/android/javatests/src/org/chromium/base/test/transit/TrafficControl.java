// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.ArrayList;
import java.util.List;

/**
 * Keeps track of all existing {@link Station}s and which one is active.
 *
 * <p>Also keeps track of which test is currently running for batched tests.
 */
public class TrafficControl {
    private static final List<Station> sAllStations = new ArrayList<>();
    private static String sCurrentTestCase;

    private static Station sActiveStation;

    static void notifyCreatedStation(Station station) {
        sAllStations.add(station);
    }

    static void notifyActiveStationChanged(Station newActiveStation) {
        assert newActiveStation.getPhase() == Phase.ACTIVE : "New active Station must be ACTIVE";
        if (sActiveStation != null) {
            assert sActiveStation.getPhase() != Phase.ACTIVE
                    : "Previously active station was not ACTIVE";
        }
        sActiveStation = newActiveStation;
    }

    public static List<Station> getAllStations() {
        return sAllStations;
    }

    public static Station getActiveStation() {
        return sActiveStation;
    }

    static void onTestStarted(String testName) {
        sCurrentTestCase = testName;
    }

    static void onTestFinished(String testName) {
        sCurrentTestCase = null;
    }

    static String getCurrentTestCase() {
        return sCurrentTestCase;
    }
}
