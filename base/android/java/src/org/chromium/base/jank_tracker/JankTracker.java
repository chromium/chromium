// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * Interface for Android UI jank tracking.
 */
public interface JankTracker {
    /**
     * Starts tracking UI jank for a specific use scenario (e.g. Tab switcher, Omnibox, etc.),
     * calling this method more than once without calling {@code finishTrackingScenario} won't do
     * anything.
     * @param scenario A value from {@link JankScenario} that specifies a use scenario.
     */
    void startTrackingScenario(@JankScenario int scenario);

    /**
     * Finishes tracking UI jank for a use scenario (e.g. Tab switcher, Omnibox, etc.). Histograms
     * for that scenario (e.g. Android.Jank.FrameDuration.Omnibox) are recorded immediately after
     * calling this method. Calling this method without calling {@code startTrackingScenario}
     * beforehand won't do anything.
     * @param scenario A value from {@link JankScenario} that specifies a use scenario.
     */
    void finishTrackingScenario(@JankScenario int scenario);
}
