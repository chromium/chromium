// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.AnyThread;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebViewPageState. */
@AnyThread
@NullMarked
public interface WebViewPageStateBoundaryInterface {
    String getUrl();

    /* WebViewPage */ InvocationHandler getPage();
}
