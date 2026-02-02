// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public interface ExecuteJavaScriptCallbackBoundaryInterface {
    @IntDef({
        ExecuteJavaScriptExceptionTypeBoundaryInterface.GENERIC,
        ExecuteJavaScriptExceptionTypeBoundaryInterface.FRAME_DESTROYED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ExecuteJavaScriptExceptionTypeBoundaryInterface {
        int GENERIC = 0;
        int FRAME_DESTROYED = 1;
    }

    void onSuccess(String result);

    void onFailure(
            @ExecuteJavaScriptExceptionTypeBoundaryInterface int type, @Nullable String message);
}
