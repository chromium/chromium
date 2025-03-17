// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebMessage. */
@NullMarked
public interface WebMessageBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    @Deprecated
    @Nullable String getData();

    /* MessagePayload */ InvocationHandler getMessagePayload();

    /* WebMessagePort */ InvocationHandler @Nullable [] getPorts();
}
