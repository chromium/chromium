// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Boundary interface for WebMessagePayload. */
@NullMarked
public interface WebMessagePayloadBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    @WebMessagePayloadType
    int getType();

    @Nullable String getAsString();

    byte[] getAsArrayBuffer();

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
            flag = true,
            value = {WebMessagePayloadType.TYPE_STRING, WebMessagePayloadType.TYPE_ARRAY_BUFFER})
    @interface WebMessagePayloadType {
        int TYPE_STRING = 0;
        int TYPE_ARRAY_BUFFER = 1;
    }
}
