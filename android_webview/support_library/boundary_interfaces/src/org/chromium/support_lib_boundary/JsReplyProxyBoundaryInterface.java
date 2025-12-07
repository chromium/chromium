// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for org.chromium.android_webview.WebMessageListener. */
@NullMarked
public interface JsReplyProxyBoundaryInterface extends IsomorphicObjectBoundaryInterface {
    /** Prefer using {@link #postMessageWithPayload}. */
    void postMessage(String message);

    void postMessageWithPayload(/* MessagePayload */ @Nullable InvocationHandler payload);
}
