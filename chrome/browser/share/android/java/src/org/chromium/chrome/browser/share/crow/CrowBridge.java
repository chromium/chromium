// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * Class providing access to functionality provided by the Crow native component.
 */
class CrowBridge {
    /** Container for past visit counts. */
    static class VisitCounts {
        /** The total number of visits. */
        public final int visits;
        /** The number of per day boolean visits (days when at least one visit happened) */
        public final int dailyVisits;

        VisitCounts(int visits, int dailyVisits) {
            this.visits = visits;
            this.dailyVisits = dailyVisits;
        }
    }

    /**
     * Obtains visit information for a website within a limited number of days in the past.
     * @param url The URL for which the host will be queried for past visits.
     * @param numDays The number of days to look back on.
     * @param callback The callback to receive the past visits query results.
     *            Upon failure, VisitCounts is populated with 0 visits.
     */
    static void getVisitCountsToHost(GURL url, int numDays, Callback<VisitCounts> callback) {
        CrowBridgeJni.get().getRecentVisitCountsToHost(
                url, numDays, (result) -> callback.onResult(new VisitCounts(result[0], result[1])));
    }

    /**
     * Returns the publication ID for a hostname; empty string if host is not on the allowlist.
     * @param host The hostname to check against the allowlist.
     */
    static String getPublicationIDFromAllowlist(String host) {
        return CrowBridgeJni.get().getPublicationIDFromAllowlist(host);
    }

    /**
     * Returns whether |host| is on the denylist.
     * @param host The hostname to check against the denylist.
     */
    static boolean denylistContainsHost(String host) {
        return CrowBridgeJni.get().denylistContainsHost(host);
    }

    @NativeMethods
    interface Natives {
        void getRecentVisitCountsToHost(GURL url, int numDays, Callback<int[]> callback);
        String getPublicationIDFromAllowlist(String host);
        boolean denylistContainsHost(String host);
    }
}
