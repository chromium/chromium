// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SETTLE_MIN_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SLIDE_ANIMATION_DURATION_MS;

import android.view.animation.Interpolator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.interpolators.Interpolators;

/** Configuration and parameter helper for Hub pane swipe animations. */
@NullMarked
public final class HubPaneSwipeAnimationConfig {
    private HubPaneSwipeAnimationConfig() {}

    /** Returns the interpolator configured for pane swipe settle transitions. */
    public static Interpolator getSwipeSettleInterpolator() {
        return ChromeFeatureList.sSwipeToSwitchPaneUseEmphasizedInterpolator.getValue()
                ? Interpolators.EMPHASIZED
                : Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR;
    }

    /**
     * Returns the maximum duration in milliseconds configured for pane swipe settle transitions.
     */
    public static long getSwipeSettleMaxDurationMs() {
        int maxDuration = ChromeFeatureList.sSwipeToSwitchPaneMaxDurationMs.getValue();
        return maxDuration > 0 ? maxDuration : PANE_SLIDE_ANIMATION_DURATION_MS;
    }

    /**
     * Returns the minimum settle duration in milliseconds scaled proportionally with max duration.
     */
    public static long getSwipeSettleMinDurationMs(long maxDurationMs) {
        if (maxDurationMs <= 0 || PANE_SLIDE_ANIMATION_DURATION_MS <= 0) {
            return 0L;
        }

        double ratio = (double) PANE_SETTLE_MIN_DURATION_MS / PANE_SLIDE_ANIMATION_DURATION_MS;
        return Math.round(maxDurationMs * ratio);
    }
}
