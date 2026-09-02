// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Configuration and feature flag helper methods for on-demand background tab loading and capture.
 */
@NullMarked
public final class OnDemandBackgroundTabCaptureConfig {
    private OnDemandBackgroundTabCaptureConfig() {}

    /** Returns whether on-demand background tab context capture is enabled. */
    public static boolean isOnDemandBackgroundTabContextCaptureEnabled() {
        return ChromeFeatureList.sOnDemandBackgroundTabContextCapture.isEnabled();
    }

    /** Returns whether background tab context capture optimization is enabled. */
    public static boolean isOptimizationEnabled() {
        return ChromeFeatureList.sOnDemandBackgroundTabContextCaptureOptimization.isEnabled();
    }

    /** Returns whether offscreen rendering is enabled for background tab capture. */
    public static boolean isOffscreenRenderingEnabled() {
        return isOptimizationEnabled();
    }

    /** Returns whether early completion on first visually non-empty paint is enabled. */
    public static boolean isEarlyFirstPaintEnabled() {
        return isOptimizationEnabled()
                && ChromeFeatureList.sOnDemandBackgroundTabEnableFirstPaint.getValue();
    }

    /** Returns whether a post-first-paint delay buffer is configured. */
    public static boolean hasFirstPaintDelay() {
        return getFirstPaintDelayMs() > 0;
    }

    /** Returns the delay in milliseconds to wait after first paint before capturing. */
    public static int getFirstPaintDelayMs() {
        return Math.max(0, ChromeFeatureList.sOnDemandBackgroundTabFirstPaintDelayMs.getValue());
    }
}
