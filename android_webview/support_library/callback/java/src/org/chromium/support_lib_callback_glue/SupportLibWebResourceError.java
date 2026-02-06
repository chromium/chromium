// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_callback_glue;

import org.chromium.android_webview.AwWebResourceError;
import org.chromium.support_lib_boundary.WebResourceErrorBoundaryInterface;

/** Adapter between {@link AwWebResourceError} and {@link WebResourceErrorBoundaryInterface}. */
public class SupportLibWebResourceError implements WebResourceErrorBoundaryInterface {
    private final AwWebResourceError mError;

    public SupportLibWebResourceError(AwWebResourceError error) {
        mError = error;
    }

    public AwWebResourceError getAwWebResourceError() {
        return mError;
    }

    @Override
    public int getErrorCode() {
        return mError.getWebviewError();
    }

    // Note: This is an internal error code that may not be stable over time.
    // It is intended purely for debugging purposes.
    @Override
    public int getDebugCode() {
        return mError.getNetError();
    }

    @Override
    public CharSequence getDescription() {
        return mError.getDescription();
    }
}
