// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.PrefetchException;

import org.chromium.support_lib_boundary.PrefetchExceptionBoundaryInterface;

public class SupportLibPrefetchException implements PrefetchExceptionBoundaryInterface {
    PrefetchException mException;

    public SupportLibPrefetchException(PrefetchException exception) {
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
