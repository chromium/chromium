// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworksTrackerTest.ShadowPlatformNetworksManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;

/**
 * Robolectric tests for {@link VisibleNetworksTracker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowPlatformNetworksManager.class, CustomShadowAsyncTask.class,
                ShadowSystemClock.class})
public class VisibleNetworksTrackerTest {
    private static final VisibleWifi VISIBLE_WIFI_1 =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", 1, 10L);
    private static final VisibleWifi VISIBLE_WIFI_2 =
            VisibleWifi.create("ssid2", "11:11:11:11:11:12", 2, 20L);
    private static final VisibleCell VISIBLE_CELL_1 = VisibleCell.builder(VisibleCell.RadioType.GSM)
                                                              .setCellId(30)
                                                              .setLocationAreaCode(31)
                                                              .setMobileCountryCode(32)
                                                              .setMobileNetworkCode(33)
                                                              .setTimestamp(30L)
                                                              .build();
    private static final VisibleCell VISIBLE_CELL_2 =
            VisibleCell.builder(VisibleCell.RadioType.CDMA)
                    .setCellId(40)
                    .setLocationAreaCode(41)
                    .setMobileCountryCode(42)
                    .setMobileNetworkCode(43)
                    .setTimestamp(40L)
                    .build();

    private static final VisibleNetworks FIRST_ALL_VISIBLE_NETWORKS =
            VisibleNetworks.create(VISIBLE_WIFI_1, VISIBLE_CELL_1,
                    new HashSet<VisibleWifi>(Arrays.asList(VISIBLE_WIFI_1, VISIBLE_WIFI_2)),
                    new HashSet<VisibleCell>(Arrays.asList(VISIBLE_CELL_1, VISIBLE_CELL_2)));
    private static final VisibleNetworks SECOND_ALL_VISIBLE_NETWORKS = VisibleNetworks.create(
            VISIBLE_WIFI_2, VISIBLE_CELL_2, new HashSet<VisibleWifi>(Arrays.asList(VISIBLE_WIFI_1)),
            new HashSet<VisibleCell>(Arrays.asList(VISIBLE_CELL_1)));
    private static final VisibleNetworks FIRST_ONLY_CONNECTED_NETWORKS =
            VisibleNetworks.create(VISIBLE_WIFI_1, VISIBLE_CELL_1, null, null);
    private static final VisibleNetworks SECOND_ONLY_CONNECTED_NETWORKS =
            VisibleNetworks.create(VISIBLE_WIFI_2, VISIBLE_CELL_2, null, null);

    private static final long CURRENT_TIME_MS = 90000000L;
    private static final long ELAPSED_UNDER_THRESHOLD_TIME_MS =
            VisibleNetworksTracker.AGE_THRESHOLD - 1000L;
    private static final long ELAPSED_OVER_THRESHOLD_TIME_MS =
            VisibleNetworksTracker.AGE_THRESHOLD + 1000L;

    @Mock
    private static Context sContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS);
        ShadowPlatformNetworksManager.sAllVisibleNetworks = new LinkedList<>(
                Arrays.asList(FIRST_ALL_VISIBLE_NETWORKS, SECOND_ALL_VISIBLE_NETWORKS));
        ShadowPlatformNetworksManager.sOnlyConnectedNetworks = new LinkedList<>(
                Arrays.asList(FIRST_ONLY_CONNECTED_NETWORKS, SECOND_ONLY_CONNECTED_NETWORKS));

        // Make sure that the cache is empty before every test.
        TestThreadUtils.runOnUiThreadBlocking(() -> { VisibleNetworksTracker.clearCache(); });
    }

    @Test
    public void testGetLastKnownVisibleNetworks_emptyCache() {
        // Cache is empty before the getLastKnownVisibleNetworks call. The visibleNetworks
        // are computed only for the connected wifi and cell and then cached.
        VisibleNetworks visibleNetworks =
                VisibleNetworksTracker.getLastKnownVisibleNetworks(sContext);
        assertEquals(FIRST_ONLY_CONNECTED_NETWORKS, visibleNetworks);

        // A background refresh should be triggered as a result, and compute all to cache them.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    @Test
    public void testGetLastKnownVisibleNetworks_withValidCache() {
        VisibleNetworks visibleNetworks =
                VisibleNetworksTracker.getLastKnownVisibleNetworks(sContext);
        assertEquals(FIRST_ONLY_CONNECTED_NETWORKS, visibleNetworks);

        // Wait until cache is populated
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Time to consider the first cached networks still as valid.
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS + ELAPSED_UNDER_THRESHOLD_TIME_MS);
        visibleNetworks = VisibleNetworksTracker.getLastKnownVisibleNetworks(sContext);

        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, visibleNetworks);
        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    @Test
    public void testGetLastKnownVisibleNetworks_withOldCache() {
        VisibleNetworks visibleNetworks =
                VisibleNetworksTracker.getLastKnownVisibleNetworks(sContext);
        assertEquals(FIRST_ONLY_CONNECTED_NETWORKS, visibleNetworks);

        // Wait until cache is populated
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Time to consider the first cached networks as invalid. Should fetch the second ones.
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS + ELAPSED_OVER_THRESHOLD_TIME_MS);
        visibleNetworks = VisibleNetworksTracker.getLastKnownVisibleNetworks(sContext);

        assertEquals(SECOND_ONLY_CONNECTED_NETWORKS, visibleNetworks);

        // A background refresh should be triggered as a result, and compute all to cache them.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(
                SECOND_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS + ELAPSED_OVER_THRESHOLD_TIME_MS,
                VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    @Test
    public void testRefreshVisibleNetworks_emptyCache() {
        // Cache is empty before the refreshVisibleNetworks call. The visibleNetworks are
        // computed including all the visible cells and wifis then cached.
        VisibleNetworksTracker.refreshVisibleNetworks(sContext);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    @Test
    public void testRefreshVisibleNetworks_validCache() {
        VisibleNetworksTracker.refreshVisibleNetworks(sContext);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());

        // Time to consider the first cached networks still as valid, refresh should be a noop.
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS + ELAPSED_UNDER_THRESHOLD_TIME_MS);
        VisibleNetworksTracker.refreshVisibleNetworks(sContext);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    @Test
    public void testRefreshVisibleNetworks_oldCache() {
        VisibleNetworksTracker.refreshVisibleNetworks(sContext);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(FIRST_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS, VisibleNetworksTracker.getCachedVisibleNetworksTime());

        // Time to consider the first cached networks as invalid. Should fetch the second ones.
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS + ELAPSED_OVER_THRESHOLD_TIME_MS);
        VisibleNetworksTracker.refreshVisibleNetworks(sContext);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(
                SECOND_ALL_VISIBLE_NETWORKS, VisibleNetworksTracker.getCachedVisibleNetworks());
        assertEquals(CURRENT_TIME_MS + ELAPSED_OVER_THRESHOLD_TIME_MS,
                VisibleNetworksTracker.getCachedVisibleNetworksTime());
    }

    /**
     * Shadow PlatformNetworksManager for robolectric tests.
     */
    @Implements(PlatformNetworksManager.class)
    public static class ShadowPlatformNetworksManager {
        private static List<VisibleNetworks> sAllVisibleNetworks;
        private static List<VisibleNetworks> sOnlyConnectedNetworks;

        @Implementation
        public static VisibleNetworks computeConnectedNetworks(Context context) {
            return sOnlyConnectedNetworks.remove(0);
        }

        @Implementation
        public static void computeVisibleNetworks(
                Context context, Callback<VisibleNetworks> callback) {
            callback.onResult(sAllVisibleNetworks.remove(0));
        }
    }
}
