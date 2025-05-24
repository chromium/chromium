// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

/** Boundary interface for WebViewRenderer. */
@NullMarked
public interface WebViewRendererBoundaryInterface extends IsomorphicObjectBoundaryInterface {
    boolean terminate();
}
