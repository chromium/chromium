// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebViewCompat.WebViewStartUpCallback. */
@NullMarked
public interface WebViewStartUpCallbackBoundaryInterface {
    void onSuccess(/* WebViewStartUpResult */ InvocationHandler result);
}
