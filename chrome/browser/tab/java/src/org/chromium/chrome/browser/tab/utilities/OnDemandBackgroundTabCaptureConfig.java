// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Configuration and feature flag helper methods for on-demand background tab loading and capture.
 */
@NullMarked
public final class OnDemandBackgroundTabCaptureConfig {
    @VisibleForTesting static final int KILOBYTES_PER_GIGABYTE = 1024 * 1024;

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

    /** Returns whether concurrent background tab loads should be limited. */
    public static boolean isLimitConcurrentLoadsEnabled() {
        return isOptimizationEnabled()
                && ChromeFeatureList.sOnDemandBackgroundTabContextCaptureLimitConcurrentLoadIfNeeded
                        .getValue();
    }

    /** Returns the minimum number of concurrent background tab loads allowed. */
    public static int getMinimumConcurrentLoads() {
        return Math.max(
                1,
                ChromeFeatureList.sOnDemandBackgroundTabContextCaptureMinimumConcurrentLoadIfNeeded
                        .getValue());
    }

    /** Returns the maximum number of concurrent background tab loads allowed. */
    public static int getMaximumConcurrentLoads() {
        return Math.max(
                getMinimumConcurrentLoads(),
                ChromeFeatureList.sOnDemandBackgroundTabContextCaptureMaximumConcurrentLoadIfNeeded
                        .getValue());
    }

    /**
     * Calculates the concurrent background tab load limit based on device hardware and feature
     * parameters.
     *
     * <p>Concurrency is constrained by the hardware bottleneck between compute and memory:
     *
     * <ul>
     *   <li>CPU: Allow roughly 1 load per 2 CPU cores to prevent saturating the CPU and starving
     *       the browser UI thread and compositor.
     *   <li>RAM: Allow at most 1 load per GB of physical RAM to avoid triggering Android's low
     *       memory killer (LMK) on memory-constrained devices.
     * </ul>
     *
     * We take the minimum to adapt to whichever constraint is more restrictive, and clamp between
     * the configured minimum and maximum limits.
     *
     * @return The maximum number of concurrent tab loads permitted on this device.
     */
    public static int getConcurrentLoadLimit() {
        int min = getMinimumConcurrentLoads();
        int max = getMaximumConcurrentLoads();
        int gb =
                Math.round(
                        (float) SysUtils.amountOfPhysicalMemoryKB() / KILOBYTES_PER_GIGABYTE);
        int cpuCores = Runtime.getRuntime().availableProcessors();
        int hardwareCapacity = Math.min(Math.max(1, cpuCores / 2), Math.max(1, gb));
        return MathUtils.clamp(hardwareCapacity, min, max);
    }
}
