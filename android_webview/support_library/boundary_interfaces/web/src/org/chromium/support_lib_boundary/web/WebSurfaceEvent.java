// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.web;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import org.chromium.support_lib_boundary.web.util.WebSurfaceEventPacker;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Event identifiers for WebSurface event callbacks. Scroll payloads are float arrays in physical
 * pixels (per second for velocities), in the direction of the finger movement, packed and unpacked
 * using {@link WebSurfaceEventPacker}.
 */
@NullMarked
@Target(ElementType.TYPE_USE)
@IntDef({
    WebSurfaceEvent.INVALIDATE,
    WebSurfaceEvent.OVER_SCROLL,
})
@Retention(RetentionPolicy.SOURCE)
public @interface WebSurfaceEvent {
    /** Reports that the WebSurface needs to be redrawn. The event payload is {@code null}. */
    int INVALIDATE = 0;

    /**
     * Unconsumed overscroll. Velocity is zero unless flinging.
     *
     * <p>Pack with {@link WebSurfaceEventPacker#packOverScroll} and unpack with {@link
     * WebSurfaceEventPacker#unpackOverScroll}.
     */
    int OVER_SCROLL = 1;
}
