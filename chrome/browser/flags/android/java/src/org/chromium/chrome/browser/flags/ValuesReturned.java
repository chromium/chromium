// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;

import java.util.HashMap;
import java.util.Map;

/**
 * Keeps track of values returned for cached flags and field trial parameters.
 */
abstract class ValuesReturned {
    @GuardedBy("sBoolValues")
    static final Map<String, Boolean> sBoolValues = new HashMap<>();
    @GuardedBy("sStringValues")
    static final Map<String, String> sStringValues = new HashMap<>();
    @GuardedBy("sIntValues")
    static final Map<String, Integer> sIntValues = new HashMap<>();
    @GuardedBy("sDoubleValues")
    static final Map<String, Double> sDoubleValues = new HashMap<>();

    @VisibleForTesting
    static void clearForTesting() {
        synchronized (sBoolValues) {
            sBoolValues.clear();
        }
        synchronized (sStringValues) {
            sStringValues.clear();
        }
        synchronized (sIntValues) {
            sIntValues.clear();
        }
        synchronized (sDoubleValues) {
            sDoubleValues.clear();
        }
    }
}
