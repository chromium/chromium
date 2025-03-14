// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebViewNavigation. */
@NullMarked
public interface WebViewNavigationBoundaryInterface extends IsomorphicObjectBoundaryInterface {
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

    /* WebViewPage */ @Nullable InvocationHandler getPage();
}
