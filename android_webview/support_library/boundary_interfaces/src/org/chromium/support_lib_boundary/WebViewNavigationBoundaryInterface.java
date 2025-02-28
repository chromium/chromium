// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

/** Boundary interface for WebViewNavigation. */
@NullMarked
public interface WebViewNavigationBoundaryInterface extends IsomorphicObjectBoundaryInterface {
    String getUrl();

    boolean isPageInitiated();

    boolean isSameDocument();

    boolean isReload();

    boolean isHistory();

    boolean isRestore();

    boolean isBack();

    boolean isForward();

    boolean hasCommitted();

    boolean didCommitErrorPage();

    int getStatusCode();
}
