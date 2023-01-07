// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.metrics;

/**
 * Data-only class to contain metrics recording constants and behaviour for Browser Services.
 */
public class BrowserServicesTimingMetrics {
    /**
     * Records a {@link TimingMetric} for the amount of time spent querying the Android
     * system for ResolveInfos that will deal with a given URL when launching from a background
     * service.
     */
    public static final String SERVICE_TAB_RESOLVE_TIME =
            "BrowserServices.ServiceTabResolveInfoQuery";

    /**
     * Records a {@link WallTimingMetric} for the amount of time spent opening the
     * {@link InstalledWebappDataRegister}.
     */
    public static final String CLIENT_APP_DATA_LOAD_TIME = "BrowserServices.ClientAppDataLoad";

    /**
     * Records a {@link WallTimingMetric} for the amount of time taken to check if a
     * package handles a Browsable intent.
     */
    public static final String BROWSABLE_INTENT_RESOLUTION_TIME =
            "BrowserServices.BrowsableIntentCheck";
}
