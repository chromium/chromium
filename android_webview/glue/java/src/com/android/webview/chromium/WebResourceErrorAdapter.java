// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebResourceError;

import org.chromium.android_webview.AwContentsClient.AwWebResourceError;

/** Chromium implementation of {@link WebResourceError}. */
public class WebResourceErrorAdapter extends WebResourceError {
    private final AwWebResourceError mError;

    public WebResourceErrorAdapter(AwWebResourceError error) {
        mError = error;
    }

    /* package */ AwWebResourceError getAwWebResourceError() {
        return mError;
    }

    @Override
    public int getErrorCode() {
        return mError.errorCode;
    }

    @Override
    public CharSequence getDescription() {
        return mError.description;
    }
}
