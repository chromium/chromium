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
class ValuesReturned {
    @GuardedBy("boolValues")
    public final Map<String, Boolean> boolValues = new HashMap<>();
    @GuardedBy("stringValues")
    public final Map<String, String> stringValues = new HashMap<>();
    @GuardedBy("intValues")
    public final Map<String, Integer> intValues = new HashMap<>();
    @GuardedBy("doubleValues")
    public final Map<String, Double> doubleValues = new HashMap<>();

    @VisibleForTesting
    final void clearForTesting() {
        synchronized (boolValues) {
            boolValues.clear();
        }
        synchronized (stringValues) {
            stringValues.clear();
        }
        synchronized (intValues) {
            intValues.clear();
        }
        synchronized (doubleValues) {
            doubleValues.clear();
        }
    }
}
