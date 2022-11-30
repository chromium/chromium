// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import android.os.Bundle;

/**
 * Interface to record UMA API calls inside non-embedded WebView processes and retrieve theses
 * records back in an embedded WebView.
 */
interface IMetricsBridgeService {
    /**
     * Record a UMA API method call from a non-embedded WebView processes. This should only be
     * called by WebView's non-embedded processes (which are trusted). This is a blocking IPC,
     * although its work (including disk IO) happens asynchronously.
     *
     * @param methodCall a byte array serialization of
     *                   org.chromium.android_webview.proto.HistogramRecord proto message object.
     */
    void recordMetrics(in byte[] methodCall);

    /**
     * Get a list of recorded UMA method calls through the callback. This a blocking call.
     * This should only be called from a process that can call UMA APIs directly (e.g embedded
     * WebView).
     *
     * @returns a List<byte[]> of byte array serialization of
                org.chromium.android_webview.proto.HistogramRecord proto message object.
     */
    List retrieveNonembeddedMetrics();
}