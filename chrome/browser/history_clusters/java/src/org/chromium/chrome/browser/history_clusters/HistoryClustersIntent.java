// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

/**
 * Class containing constants related to intents that show the Journeys UI in the History activity.
 */
public class HistoryClustersIntent {
    /** Extra specifying that the Journeys UI should be shown. */
    public static final String EXTRA_SHOW_HISTORY_CLUSTERS =
            "org.chromium.chrome.browser.history_clusters.show";

    /** Extra specifying what the preset query for the journeys UI should be. */
    public static final String EXTRA_HISTORY_CLUSTERS_QUERY =
            "org.chromium.chrome.browser.history_clusters.query";
}
