// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.util.List;

/** Boundary interface for WebViewStartUpResult. */
public interface WebViewStartUpResultBoundaryInterface {
    Long getTotalTimeInUiThreadMillis();

    Long getMaxTimePerTaskInUiThreadMillis();

    List<Throwable> getBlockingStartUpLocations();
}
