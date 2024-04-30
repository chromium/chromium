// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

/**
 * Utility class for scaling various timeouts by a common factor.
 *
 * <p>Set this value via command-line. E.g.: out/Debug/bin/run_tests --timeout-scale=3
 */
public class ScalableTimeout {
    private static float sTimeoutScale = 1;

    public static void setScale(float value) {
        sTimeoutScale = value;
    }

    public static long scaleTimeout(long timeout) {
        return (long) (timeout * sTimeoutScale);
    }
}
