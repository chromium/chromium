// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

/** Boundary interface for HttpCache. */
@NullMarked
public interface HttpCacheBoundaryInterface extends IsomorphicObjectBoundaryInterface {
    long getDefaultQuotaBytes();

    boolean isUsingDefaultQuota();

    void useDefaultQuota();

    long getQuotaBytes();

    void setQuotaBytes(long quotaInBytes);
}
