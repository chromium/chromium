// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * Dummy implementation of JankTracker.
 */
public class DummyJankTracker implements JankTracker {
    @Override
    public void startTrackingScenario(int scenario) {}

    @Override
    public void finishTrackingScenario(int scenario) {}
}
