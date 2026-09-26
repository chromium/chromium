// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Event identifiers for WebSurface event callbacks. */
@NullMarked
@Target(ElementType.TYPE_USE)
@IntDef({
    WebSurfaceEvent.INVALIDATE,
})
@Retention(RetentionPolicy.SOURCE)
public @interface WebSurfaceEvent {
    int INVALIDATE = 0;
}
