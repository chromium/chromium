// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public interface PrefetchOperationCallbackBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    @IntDef({
        PrefetchExceptionTypeBoundaryInterface.GENERIC,
        PrefetchExceptionTypeBoundaryInterface.NETWORK,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PrefetchExceptionTypeBoundaryInterface {
        int GENERIC = 0;
        int NETWORK = 1;
    }

    @IntDef({
        PrefetchResultTypeBoundaryInterface.SUCCESS,
        PrefetchResultTypeBoundaryInterface.DUPLICATE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PrefetchResultTypeBoundaryInterface {
        int SUCCESS = 0;
        int DUPLICATE = 1;
    }

    void onResult(@PrefetchResultTypeBoundaryInterface int type);

    @Deprecated
    default void onSuccess() {
        // onSuccess is deprecated. Use onResult instead
        throw new UnsupportedOperationException("http://crbug.com/483041824 Replaced by onResult.");
    }

    void onFailure(
            @PrefetchExceptionTypeBoundaryInterface int type, String message, int networkErrorCode);
}
