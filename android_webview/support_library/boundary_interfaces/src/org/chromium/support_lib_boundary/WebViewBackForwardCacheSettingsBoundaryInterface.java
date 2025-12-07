// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

/** Boundary interface for WebViewBackForwardCacheSettings. */
@NullMarked
public interface WebViewBackForwardCacheSettingsBoundaryInterface
        extends IsomorphicObjectBoundaryInterface {
    int getTimeoutInSeconds();

    int getMaxPagesInCache();
}
