// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import androidx.annotation.NonNull;

import com.android.webview.chromium.PrefetchDuplicateException;

import org.chromium.support_lib_boundary.PrefetchDuplicateExceptionBoundaryInterface;

public class SupportLibPrefetchDuplicateException
        implements PrefetchDuplicateExceptionBoundaryInterface {
    PrefetchDuplicateException mException;

    public SupportLibPrefetchDuplicateException(@NonNull PrefetchDuplicateException exception) {
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
}
