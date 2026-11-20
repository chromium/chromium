// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebView;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for DownloadHandler. */
@NullMarked
public interface DownloadHandlerBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    void onDownloadRequested(WebView webView, /* WebDownload */ InvocationHandler download);
}
