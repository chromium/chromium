// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public interface PrefetchOperationCallbackBoundaryInterface {
    @IntDef({
        PrefetchExceptionTypeBoundaryInterface.GENERIC,
        PrefetchExceptionTypeBoundaryInterface.NETWORK,
        PrefetchExceptionTypeBoundaryInterface.DUPLICATE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PrefetchExceptionTypeBoundaryInterface {
        int GENERIC = 0;
        int NETWORK = 1;
        int DUPLICATE = 2;
    }

    void onSuccess();

    void onFailure(
            @PrefetchExceptionTypeBoundaryInterface int type, String message, int networkErrorCode);
}
