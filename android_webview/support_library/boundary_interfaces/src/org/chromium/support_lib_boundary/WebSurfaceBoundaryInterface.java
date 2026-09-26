// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.graphics.Canvas;
import android.view.MotionEvent;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;

/**
 * Boundary interface for WebSurface, which represents a presentation surface for WebContent in
 * Jetpack Compose without intermediate View hierarchy nodes.
 */
@NullMarked
public interface WebSurfaceBoundaryInterface {
    void setWebContent(@Nullable /* WebContentBoundaryInterface */ InvocationHandler webContent);

    void draw(Canvas canvas);

    void setSize(int width, int height);

    boolean onTouchEvent(MotionEvent event);

    void destroy();
}
