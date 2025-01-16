// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebMessagePort.WebMessageCallback. */
@NullMarked
public interface WebMessageCallbackBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    void onMessage(
            /* WebMessagePort */ InvocationHandler port,
            /* WebMessage */ InvocationHandler message);
}
