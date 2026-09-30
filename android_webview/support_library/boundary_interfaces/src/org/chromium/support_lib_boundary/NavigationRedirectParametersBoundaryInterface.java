// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.util.Map;

/** Boundary interface for NavigationRedirectParameters. */
@NullMarked
public interface NavigationRedirectParametersBoundaryInterface {
    Map<String, String> getResponseHeaders();

    int getStatusCode();
}
