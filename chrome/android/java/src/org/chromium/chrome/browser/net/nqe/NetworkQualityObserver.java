// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.nqe;

import org.chromium.net.EffectiveConnectionType;

/**
 * Interface for observing changes to the current Network Quality Estimate.
 */
public interface NetworkQualityObserver {
    /**
     * Called when there is a change in the effective connection type.
     *
     * @param effectiveConnectionType the current effective connection type.
     */
    default void onEffectiveConnectionTypeChanged(
            @EffectiveConnectionType int effectiveConnectionType) {}

    /**
     * Called when there is a substantial change in either HTTP RTT, transport RTT or downstream
     * throughput estimate.
     *
     * @param httpRTTMillis estimate of the round trip time at the http layer.
     * @param transportRTTMillis estimate of the round trip time at the transport layer.
     * @param downstreamThroughputKbps estimate of the downstream throughput in Kbps (Kilobits per
     *            second).
     */
    default void onRTTOrThroughputEstimatesComputed(
            long httpRTTMillis, long transportRTTMillis, int downstreamThroughputKbps) {}
}