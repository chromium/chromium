// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.Map;

/** Boundary interface for PrefetchParams. */
public interface PrefetchParamsBoundaryInterface {

    @NonNull
    Map<String, String> getAdditionalHeaders();

    @Nullable
    /* NoVarySearchDataBoundaryInterface */ InvocationHandler getNoVarySearchData();
}
