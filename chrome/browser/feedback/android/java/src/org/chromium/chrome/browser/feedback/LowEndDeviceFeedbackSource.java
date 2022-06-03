// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.base.SysUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Provides data about whether the feedback comes from a low-end device.
 */
public class LowEndDeviceFeedbackSource implements FeedbackSource {
    private static final String LOW_END_DEVICE_KEY = "lowmem";
    private final HashMap<String, String> mMap;

    LowEndDeviceFeedbackSource() {
        mMap = new HashMap<String, String>(1);
        mMap.put(LOW_END_DEVICE_KEY, Boolean.toString(SysUtils.isLowEndDevice()));
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
