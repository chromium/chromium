// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import androidx.annotation.NonNull;

import com.android.webview.chromium.NoVarySearchData;
import com.android.webview.chromium.PrefetchParams;

import org.chromium.support_lib_boundary.NoVarySearchDataBoundaryInterface;
import org.chromium.support_lib_boundary.SpeculativeLoadingParametersBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

/** Adapter between SpeculativeLoadingParametersBoundaryInterface and PrefetchParams. */
public class SupportLibSpeculativeLoadingParametersAdapter {

    private SupportLibSpeculativeLoadingParametersAdapter() {}

    public static PrefetchParams fromSpeculativeLoadingParametersBoundaryInterface(
            @NonNull SpeculativeLoadingParametersBoundaryInterface speculativeLoadingParams) {
        NoVarySearchDataBoundaryInterface noVarySearchData =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        NoVarySearchDataBoundaryInterface.class,
                        speculativeLoadingParams.getNoVarySearchData());

        return new PrefetchParams(
                speculativeLoadingParams.getAdditionalHeaders(),
                fromNoVarySearchDataBoundaryInterface(noVarySearchData),
                speculativeLoadingParams.isJavaScriptEnabled());
    }

    public static NoVarySearchData fromNoVarySearchDataBoundaryInterface(
            NoVarySearchDataBoundaryInterface noVarySearchData) {
        if (noVarySearchData == null) return null;
        return new NoVarySearchData(
                noVarySearchData.getVaryOnKeyOrder(),
                noVarySearchData.getIgnoreDifferencesInParameters(),
                noVarySearchData.getIgnoredQueryParameters(),
                noVarySearchData.getConsideredQueryParameters());
    }
}
