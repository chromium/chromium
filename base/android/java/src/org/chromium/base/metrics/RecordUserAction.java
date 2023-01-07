// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import org.chromium.base.TimeUtils;

/**
 * Java API for recording UMA actions.
 * <p>
 * WARNINGS: JNI calls are relatively costly - avoid using in performance-critical code.
 * <p>
 * Action names must be documented in {@code actions.xml}. See {@link
 * https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/actions/README.md} <p>
 * We use a script ({@code extract_actions.py{}) to scan the source code and extract actions. A
 * string literal (not a variable) must be passed to {@link #record(String)}.
 */
public class RecordUserAction {
    /**
     * Similar to {@code base::RecordAction()} in C++.
     * <p>
     * Record that the user performed an action. See tools/metrics/actions/README.md
     */
    public static void record(final String action) {
        UmaRecorderHolder.get().recordUserAction(action, TimeUtils.elapsedRealtimeMillis());
    }
}
