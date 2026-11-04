// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.graphics.Color;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Delegate to provide proprietary branding details for subscription tiers (e.g. AI Premium). The
 * concrete implementation is provided by downstream targets via ServiceLoaderUtil.
 */
@NullMarked
public interface SubscriptionTierBrandingDelegate {
    /**
     * Returns a Shader fully configured for the AI tier ring.
     *
     * @param bounds The bounds of the ring to be drawn.
     * @return The Shader to use for drawing the ring.
     */
    @Nullable Shader getRingShader(RectF bounds);

    /**
     * Returns the Shader for the wave at the bottom of the AI tier pill, or null for no wave.
     *
     * @param bounds The bounds of the pill, starting at (0, 0), i.e. {@code (0, 0, width, height)}.
     */
    default @Nullable Shader getTierPillWaveShader(RectF bounds) {
        return null;
    }

    /**
     * Returns the wave at the bottom of the AI tier pill, scaled to {@code bounds}, or null for no
     * wave.
     *
     * @param bounds The bounds of the pill, starting at (0, 0), i.e. {@code (0, 0, width, height)}.
     */
    default @Nullable Path getTierPillWavePath(RectF bounds) {
        return null;
    }

    /**
     * Returns the translucent color of the inner shadow of the AI tier pill, or transparent for no
     * shadow.
     */
    default @ColorInt int getTierPillShadowColor() {
        return Color.TRANSPARENT;
    }
}
