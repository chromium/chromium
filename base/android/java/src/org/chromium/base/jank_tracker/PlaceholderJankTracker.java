// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/** Placeholder implementation of JankTracker. */
public class PlaceholderJankTracker implements JankTracker {
    @Override
    public void startTrackingScenario(int scenario) {}

    @Override
    public void finishTrackingScenario(int scenario) {}

    @Override
    public void finishTrackingScenario(int scenario, long endScenarioTimeNs) {}

    @Override
    public void destroy() {}
}
