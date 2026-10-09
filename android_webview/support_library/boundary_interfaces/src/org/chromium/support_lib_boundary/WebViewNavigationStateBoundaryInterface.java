// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.AnyThread;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.Map;

/** Boundary interface for WebViewNavigationState. */
@AnyThread
@NullMarked
public interface WebViewNavigationStateBoundaryInterface {
    String getUrl();

    boolean wasInitiatedByPage();

    boolean isSameDocument();

    boolean isReload();

    boolean isHistory();

    boolean isRestore();

    boolean isBack();

    boolean isForward();

    boolean didCommit();

    boolean didCommitErrorPage();

    int getStatusCode();

    long getNavigationStartUptimeMillis();

    @Nullable /* WebViewPageState */ InvocationHandler getPageState();

    @Nullable /* WebResourceError */ InvocationHandler getWebResourceError();

    @Nullable Map<String, String> getResponseHeaders();

    /* WebViewNavigation */ InvocationHandler getNavigation();
}
