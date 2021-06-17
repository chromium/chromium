// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import java.util.HashMap;
import java.util.Map;

/**
 * Keeps track of values returned for cached flags and field trial parameters.
 */
class ValuesReturned {
    private Map<String, Boolean> mBoolValues = new HashMap<>();
    private Map<String, String> mStringValues = new HashMap<>();
    private Map<String, Integer> mIntValues = new HashMap<>();
    private Map<String, Double> mDoubleValues = new HashMap<>();

    Map<String, Boolean> boolMap() {
        return mBoolValues;
    }

    Map<String, String> stringMap() {
        return mStringValues;
    }

    Map<String, Integer> intMap() {
        return mIntValues;
    }

    Map<String, Double> doubleMap() {
        return mDoubleValues;
    }

    void clear() {
        mBoolValues.clear();
        mStringValues.clear();
        mIntValues.clear();
        mDoubleValues.clear();
    }
}
