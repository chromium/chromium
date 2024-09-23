// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Boundary interface for PrefetchOperationResultStatusCode, that has to be in sync with Chromium's
 * PrefetchOperationStatusCode.
 */
@IntDef({PrefetchStatusCodeBoundaryInterface.SUCCESS, PrefetchStatusCodeBoundaryInterface.FAILURE})
@Retention(RetentionPolicy.SOURCE)
public @interface PrefetchStatusCodeBoundaryInterface {
    int SUCCESS = 0;
    int FAILURE = 1;
}
