// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.content.Context;
import android.os.SystemClock;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;

import javax.annotation.Nullable;

/**
 * VisibleNetworksTracker keeps track of the visible networks.
 */
class VisibleNetworksTracker {
    private static final String TAG = "VNTracker";

    @VisibleForTesting
    static final int AGE_THRESHOLD = 5 * 60 * 1000; // 5 min

    @Nullable
    private static VisibleNetworks sVisibleNetworks;
    private static long sVisibleNetworksTime = Long.MAX_VALUE;

    @Nullable
    private static AsyncTask<VisibleNetworks> sOngoingRefresh;

    private static VisibleNetworks sVisibleNetworksForTesting;
    private static boolean sUseVisibleNetworksForTesting;

    /**
     * Returns last known visible networks. It returns the cached value if the cache is valid or it
     * computes the simplest possible visibleNetworks fast, and triggers a background asynchronous
     * refresh. Might return null if visible networks cannot be computed.
     */
    @Nullable
    static VisibleNetworks getLastKnownVisibleNetworks(final Context context) {
        if (sUseVisibleNetworksForTesting) return sVisibleNetworksForTesting;

        if (isValidCachedVisibleNetworks()) return getCachedVisibleNetworks();

        VisibleNetworks visibleNetworks = null;
        try {
            // Include only the connected cell/wifi to minimize latency and compute the simplest
            // visible networks possible.
            visibleNetworks = PlatformNetworksManager.computeVisibleNetworks(
                    context, false /* includeAllVisibleNotConnectedNetworks */);
        } catch (Exception e) {
            Log.e(TAG, "Failed to get the visible networks. Error: ", e.toString());
        }
        // Update cache asynchronously.
        refreshVisibleNetworks(context);

        return visibleNetworks;
    }

    /**
     * Determines if the visible networks need to be refreshed and asynchronously updates them if
     * needed.
     */
    static void refreshVisibleNetworks(final Context context) {
        ThreadUtils.assertOnUiThread();
        if (isValidCachedVisibleNetworks() || sOngoingRefresh != null) {
            return;
        }
        sOngoingRefresh = new AsyncTask<VisibleNetworks>() {
            @Override
            protected VisibleNetworks doInBackground() {
                VisibleNetworks visibleNetworks = null;
                try {
                    // Include all visible wifis and cells.
                    visibleNetworks = PlatformNetworksManager.computeVisibleNetworks(
                            context, true /* includeAllVisibleNotConnectedNetworks */);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to get the visible networks. Error: ", e.toString());
                }
                return visibleNetworks;
            }

            @Override
            protected void onPostExecute(VisibleNetworks visibleNetworks) {
                sOngoingRefresh = null;
                setCachedVisibleNetworks(visibleNetworks);
            }
        };
        sOngoingRefresh.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Nullable
    @VisibleForTesting
    static VisibleNetworks getCachedVisibleNetworks() {
        return sVisibleNetworks;
    }

    @VisibleForTesting
    static long getCachedVisibleNetworksTime() {
        return sVisibleNetworksTime;
    }

    @VisibleForTesting
    static void clearCache() {
        setCachedVisibleNetworks(null);
        sVisibleNetworksTime = Long.MAX_VALUE;
    }

    @VisibleForTesting
    static void setVisibleNetworksForTesting(VisibleNetworks visibleNetworksForTesting) {
        sVisibleNetworksForTesting = visibleNetworksForTesting;
        sUseVisibleNetworksForTesting = true;
    }

    private static void setCachedVisibleNetworks(VisibleNetworks visibleNetworks) {
        ThreadUtils.assertOnUiThread();
        sVisibleNetworks = visibleNetworks;
        sVisibleNetworksTime = SystemClock.elapsedRealtime();
    }

    private static boolean isValidCachedVisibleNetworks() {
        return sVisibleNetworks != null && sVisibleNetworksTime != Long.MAX_VALUE
                && !sVisibleNetworks.isEmpty()
                && SystemClock.elapsedRealtime() - sVisibleNetworksTime < AGE_THRESHOLD;
    }
}