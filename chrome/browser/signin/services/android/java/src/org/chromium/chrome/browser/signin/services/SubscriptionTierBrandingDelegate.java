// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.graphics.RectF;
import android.graphics.Shader;

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
}
