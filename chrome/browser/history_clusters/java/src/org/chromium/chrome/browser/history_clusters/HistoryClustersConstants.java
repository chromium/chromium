// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

/**
 * Public class defining HistoryClusters-related constants of external interest, e.g. for
 * extracting data from intents and URIs constructed to display the History Clusters UI in some
 * externally-owned surface.
 */
public class HistoryClustersConstants {
    /** Path portion of a URI that when matched signifies that the journeys UI should be shown. */
    public static final String JOURNEYS_PATH = "journeys";
    /** Query parameter key of a URI that specifies the preset query for the Journeys UI. */
    public static final String HISTORY_CLUSTERS_QUERY_KEY = "q";

    /** Extra specifying that the Journeys UI should be shown. */
    public static final String EXTRA_SHOW_HISTORY_CLUSTERS =
            "org.chromium.chrome.browser.history_clusters.show";

    /** Extra specifying what the preset query for the journeys UI should be. */
    public static final String EXTRA_HISTORY_CLUSTERS_QUERY =
            "org.chromium.chrome.browser.history_clusters.query";
}
