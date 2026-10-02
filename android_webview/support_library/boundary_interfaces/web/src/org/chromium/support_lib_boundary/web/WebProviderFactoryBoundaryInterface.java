// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.web;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** Boundary interface for Web globals and singletons. */
@NullMarked
public interface WebProviderFactoryBoundaryInterface {
    /* WebContentBoundaryInterface */ InvocationHandler buildWebContent(
            /* Config= */ Consumer<BiConsumer<@WebContentConfig Integer, Object>> buildConfig);

    /* WebSurfaceBoundaryInterface */ InvocationHandler createWebSurface(
            BiConsumer<@WebSurfaceEvent Integer, Object> eventListener);

    String[] getSupportedFeatures();
}
