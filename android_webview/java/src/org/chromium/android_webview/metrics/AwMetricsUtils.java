// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import org.chromium.base.ApiCompatibilityUtils;

import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/** Contains Metrics related utility functions. */
public final class AwMetricsUtils {
    private AwMetricsUtils() {}

    /**
     * Replicates the algorithm used to hash histogram names to avoid the complexity of JNI.
     * The native implementation is present in base/metrics/metrics_hashes.cc
     */
    public static long hashHistogramName(String histogramName) {
        try {
            MessageDigest md = MessageDigest.getInstance("MD5");
            byte[] bytes = md.digest(ApiCompatibilityUtils.getBytesUtf8(histogramName));
            return ByteBuffer.wrap(Arrays.copyOfRange(bytes, 0, 8)).getLong();
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }
}
