// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.Map;

/** Boundary interface for PrefetchParams. */
@NullMarked
public interface SpeculativeLoadingParametersBoundaryInterface {

    Map<String, String> getAdditionalHeaders();

    @Nullable /* NoVarySearchDataBoundaryInterface */ InvocationHandler getNoVarySearchData();

    boolean isJavaScriptEnabled();
}
