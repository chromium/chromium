// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import androidx.annotation.NonNull;

import com.android.webview.chromium.PrefetchNetworkException;

import org.chromium.support_lib_boundary.PrefetchNetworkExceptionBoundaryInterface;

public class SupportLibPrefetchNetworkException
        implements PrefetchNetworkExceptionBoundaryInterface {
    PrefetchNetworkException mException;

    public SupportLibPrefetchNetworkException(@NonNull PrefetchNetworkException exception) {
        mException = exception;
    }

    @Override
    public String getMessage() {
        return mException.getMessage();
    }

    @Override
    public Throwable getCause() {
        return mException.getCause();
    }

    @Override
    public int getHttpResponseStatusCode() {
        return mException.getHttpResponseStatusCode();
    }
}
