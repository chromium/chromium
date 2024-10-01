// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.Nullable;

import java.util.List;

/** Boundary Interface for NoVarySearchData */
public interface NoVarySearchDataBoundaryInterface {

    boolean getVaryOnKeyOrder();

    boolean getIgnoreDifferencesInParameters();

    @Nullable
    List<String> getIgnoredQueryParameters();

    @Nullable
    List<String> getConsideredQueryParameters();
}
