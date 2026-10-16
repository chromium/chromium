// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.web.util;

import org.jspecify.annotations.NullMarked;

import org.chromium.support_lib_boundary.web.WebSurfaceEvent;

/**
 * Utility methods for packing and unpacking {@link WebSurfaceEvent} payloads across the support
 * library boundary.
 */
@NullMarked
public final class WebSurfaceEventPacker {
    private WebSurfaceEventPacker() {}

    /**
     * Packs an {@link WebSurfaceEvent#OVER_SCROLL} payload into a {@code float[]} containing {@code
     * {deltaX, deltaY, velocityX, velocityY}}.
     */
    public static float[] packOverScroll(
            float deltaX, float deltaY, float velocityX, float velocityY) {
        return new float[] {deltaX, deltaY, velocityX, velocityY};
    }

    /**
     * Unpacks an {@link WebSurfaceEvent#OVER_SCROLL} payload into a {@code float[]} containing
     * {@code {deltaX, deltaY, velocityX, velocityY}}.
     */
    public static float[] unpackOverScroll(Object payload) {
        return (float[]) payload;
    }
}
