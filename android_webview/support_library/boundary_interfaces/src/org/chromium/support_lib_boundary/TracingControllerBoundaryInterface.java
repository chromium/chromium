// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.io.OutputStream;
import java.util.Collection;
import java.util.concurrent.Executor;

/**
 * Boundary interface for TracingController.
 */
public interface TracingControllerBoundaryInterface {
    boolean isTracing();

    void start(int predefinedCategories, Collection<String> customIncludedCategories, int mode)
            throws IllegalStateException, IllegalArgumentException;

    boolean stop(OutputStream outputStream, Executor executor);
}