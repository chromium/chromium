// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.util.List;

/** Boundary Interface for NoVarySearchData */
@NullMarked
public interface NoVarySearchDataBoundaryInterface {

    boolean getVaryOnKeyOrder();

    boolean getIgnoreDifferencesInParameters();

    List<String> getIgnoredQueryParameters();

    List<String> getConsideredQueryParameters();
}
