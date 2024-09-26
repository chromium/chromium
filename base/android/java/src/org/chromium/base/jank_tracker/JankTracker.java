// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/** Interface for Android UI jank tracking. */
public interface JankTracker {
    /**
     * Starts tracking UI jank for a specific use scenario (e.g. Tab switcher, Omnibox, etc.),
     * calling this method more than once without calling {@code finishTrackingScenario} won't do
     * anything.
     *
     * @param scenario A value constructed from {@link JankScenarioType} along with an id that
     *     specifies a use scenario.
     */
    void startTrackingScenario(JankScenario scenario);

    /**
     * Finishes tracking UI jank for a use scenario (e.g. Tab switcher, Omnibox, etc.). Histograms
     * for that scenario (e.g. Android.Jank.FrameDuration.Omnibox) are recorded immediately after
     * calling this method. Calling this method without calling {@code startTrackingScenario}
     * beforehand won't do anything.
     *
     * @param scenario A value constructed from {@link JankScenarioType} along with an id that
     *     specifies a use scenario.
     * @param endScenarioTimeNs A value that determines the maximum frame metric (based on vsync
     *     time) that should be included.
     */
    void finishTrackingScenario(JankScenario scenario, long endScenarioTimeNs);

    void finishTrackingScenario(JankScenario scenario);

    /** To be called when the jank tracker should stop listening to changes. */
    void destroy();
}
