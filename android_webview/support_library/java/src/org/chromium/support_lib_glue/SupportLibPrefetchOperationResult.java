// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.PrefetchOperationResult;
import com.android.webview.chromium.PrefetchOperationStatusCode;

import org.chromium.support_lib_boundary.PrefetchOperationResultBoundaryInterface;
import org.chromium.support_lib_boundary.PrefetchStatusCodeBoundaryInterface;

/** Adapter between PrefetchOperationResultBoundaryInterface and PrefetchOperationResult. */
public class SupportLibPrefetchOperationResult implements PrefetchOperationResultBoundaryInterface {

    PrefetchOperationResult mPrefetchResult;

    public SupportLibPrefetchOperationResult(PrefetchOperationResult prefetchOperationResult) {
        this.mPrefetchResult = prefetchOperationResult;
    }

    @Override
    public @PrefetchStatusCodeBoundaryInterface int getStatusCode() {
        switch (mPrefetchResult.statusCode) {
            case PrefetchOperationStatusCode.SUCCESS -> {
                return PrefetchStatusCodeBoundaryInterface.SUCCESS;
            }
            case PrefetchOperationStatusCode.FAILURE -> {
                return PrefetchStatusCodeBoundaryInterface.FAILURE;
            }
        }
        throw new UnsupportedOperationException("Value of StatusCode is not supported.");
    }
}
