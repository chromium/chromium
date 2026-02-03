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

        Integer variationsId = null;
        try {
            variationsId = speculativeLoadingParams.getVariationsId();
        } catch (LinkageError | RuntimeException e) {
            // This can happen if the app is linked against an older version of the
            // Support Library boundary interfaces that does not yet include this
            // method. We catch LinkageError to handle NoSuchMethodError or
            // AbstractMethodError caused by version skew, and RuntimeException to
            // handle any unexpected reflection-related issues.
        }

        return new PrefetchParams(
                speculativeLoadingParams.getAdditionalHeaders(),
                fromNoVarySearchDataBoundaryInterface(noVarySearchData),
                speculativeLoadingParams.isJavaScriptEnabled(),
                variationsId);
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
