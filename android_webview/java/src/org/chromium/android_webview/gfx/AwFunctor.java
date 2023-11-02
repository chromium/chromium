// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;

/**
 * Interface for functor implementation. This allows client to avoid differentiating between GL and
 * Vulkan implementations.
 */
public interface AwFunctor {
    /** Insert draw functor into recording canvas */
    boolean requestDraw(Canvas canvas);

    /** Return the raw native pointer to CompositorFrameConsumer */
    long getNativeCompositorFrameConsumer();

    /** Free memory */
    void trimMemory();

    /** Destroy on UI thread. Client should stop using CompositorFrameConsumer before this */
    void destroy();
}
