// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.os.Handler;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebMessagePort. */
@NullMarked
public interface WebMessagePortBoundaryInterface {
    void postMessage(/* WebMessage */ InvocationHandler message);

    void close();

    void setWebMessageCallback(/* WebMessageCallback */ InvocationHandler callback);

    void setWebMessageCallback(
            /* WebMessageCallback */ InvocationHandler callback, @Nullable Handler handler);
}
