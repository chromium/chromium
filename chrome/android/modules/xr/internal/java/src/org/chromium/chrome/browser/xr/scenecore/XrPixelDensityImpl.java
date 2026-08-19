// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.util.DisplayMetrics;

import androidx.annotation.VisibleForTesting;
import androidx.xr.scenecore.runtime.extensions.XrExtensionsProvider;

import com.android.extensions.xr.XrExtensions;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPixelDensity;

/** Implementation of {@link XrPixelDensity}. */
@NullMarked
public final class XrPixelDensityImpl implements XrPixelDensity {
    private static final String TAG = "XrPixelDensity";
    public static final float DEFAULT_PIXELS_PER_METER = 1000f;

    private final float mDpPerMeter;
    private final float mPixelsPerMeter;

    private XrPixelDensityImpl(float dpPerMeter, float pixelsPerMeter) {
        mDpPerMeter = dpPerMeter;
        mPixelsPerMeter = pixelsPerMeter;
    }

    /**
     * Creates a {@link XrPixelDensity} instance based on the display density.
     *
     * @param displayDensity The display density in pixels per inch (PPI).
     * @return A {@link XrPixelDensity} instance.
     */
    public static XrPixelDensity create(float displayDensity) {
        float ppm = calculatePixelsPerMeter();
        float effectiveDensity = displayDensity > 0f ? displayDensity : getStableDensity();
        float dpPerMeter = ppm / effectiveDensity;
        return new XrPixelDensityImpl(dpPerMeter, ppm);
    }

    @VisibleForTesting
    public static XrPixelDensity createForTesting(float dpPerMeter, float pixelsPerMeter) {
        return new XrPixelDensityImpl(dpPerMeter, pixelsPerMeter);
    }

    private static float getStableDensity() {
        return (float) DisplayMetrics.DENSITY_DEVICE_STABLE
                / (float) DisplayMetrics.DENSITY_DEFAULT;
    }

    /**
     * Returns the default number of virtual pixels that represent one meter in ActivitySpace.
     *
     * <p>This value is a static property of the hardware and ignores user-level display preference
     * overrides (system density changes), matching AndroidX XR SceneCore RuntimeUtils.
     */
    public static float calculatePixelsPerMeter() {
        try {
            XrExtensions extensions = XrExtensionsProvider.getXrExtensions();
            if (extensions != null) {
                float result =
                        extensions.getApiVersion() > 2
                                ? extensions
                                        .getUnderlyingObject()
                                        .getConfig()
                                        .defaultPixelsPerMeter()
                                : extensions.getConfig().defaultPixelsPerMeter(getStableDensity());
                if (result > 0f) {
                    return result;
                }
            }
        } catch (Throwable t) {
            Log.w(TAG, "Failed to query XrExtensions defaultPixelsPerMeter", t);
        }
        return DEFAULT_PIXELS_PER_METER;
    }

    @Override
    public float getDpPerMeter() {
        return mDpPerMeter;
    }

    @Override
    public float getPixelsPerMeter() {
        return mPixelsPerMeter;
    }

    @Override
    public float convertDpToMeters(float dp) {
        if (mDpPerMeter <= 0f) {
            return 0f;
        }
        return dp / mDpPerMeter;
    }

    @Override
    public float convertMetersToDp(float meters) {
        return meters * mDpPerMeter;
    }

    @Override
    public float convertPixelsToMeters(float pixels) {
        if (mPixelsPerMeter <= 0f) {
            return 0f;
        }
        return pixels / mPixelsPerMeter;
    }

    @Override
    public float convertMetersToPixels(float meters) {
        return meters * mPixelsPerMeter;
    }
}
